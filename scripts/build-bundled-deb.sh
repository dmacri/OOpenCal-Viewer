#!/usr/bin/env bash

# -----------------------------------------------------------------------------
# build-bundled-deb.sh
# Builds a self-contained Debian package of OOpenCal-Viewer with all dependencies.
# - Compiles with CMake/Ninja
# - Collects runtime .so dependencies recursively (via ldd + ldconfig)
# - Copies them to usr/lib/OOpenCal-Viewer inside .deb
# - Adjusts all RPATH/RUNPATH entries to point to local lib dir
# - Produces ready-to-install .deb package (no external OpenMPI/hwloc deps)
#
# IMPORTANT:
#  - Do NOT bundle glibc loader (ld-linux) or libc unless you fully understand
#    the consequences. This script filters core system loader libs out.
#  - The script assumes you build on the same architecture as you'll run the .deb.
# -----------------------------------------------------------------------------
set -euo pipefail

# === 🩺 DEBUG TRACER ===
LOG_FILE="/tmp/build-bundled-deb.log"
exec 3>&1 1> >(tee -a "$LOG_FILE") 2>&1

# Print each command before executing
PS4='+ [${BASH_SOURCE}:${LINENO}] ${FUNCNAME[0]:+${FUNCNAME[0]}(): }'

# Trap all errors to print a detailed message
trap 'echo "❌ ERROR at line ${LINENO}: command \"${BASH_COMMAND}\" failed" >&2' ERR

echo "=== 🩺 Debug log: $LOG_FILE ==="
echo

APP_NAME="OOpenCal-Viewer"
BUILD_DIR="build"
DEB_DIR="debian/${APP_NAME}"
LIB_DIR="${DEB_DIR}/usr/lib/${APP_NAME}"
BIN_DIR="${DEB_DIR}/usr/bin"
ICON_DIR="${DEB_DIR}/usr/share/icons/hicolor/256x256/apps"
DESKTOP_DIR="${DEB_DIR}/usr/share/applications"
DEB_FILE="${APP_NAME}-bundled.deb"

# === Ensure necessary tools are available ===
echo "=== 🔧 Ensuring required build tools are present ==="
sudo apt update -y
sudo apt install -y --no-install-recommends \
  build-essential cmake ninja-build \
  qtbase5-dev qttools5-dev qttools5-dev-tools \
  libvtk9-dev libvtk9-qt-dev \
  pax-utils chrpath patchelf rsync dpkg-dev

# -----------------------------------------------------------------------------
# 🏗 Build the app
# -----------------------------------------------------------------------------
echo "=== 🏗 Building ${APP_NAME} ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# IMPORTANT: escape $ORIGIN here so CMake embeds $ORIGIN literally (not expanded locally)
cmake .. \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SKIP_RPATH=OFF \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
  -DCMAKE_INSTALL_RPATH='\$ORIGIN/../lib/'"${APP_NAME}" \
  -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON

cmake --build . --parallel
cd - >/dev/null

# -----------------------------------------------------------------------------
# 📁 Prepare Debian package structure
# -----------------------------------------------------------------------------
echo "=== 📁 Preparing Debian directory tree ==="
rm -rf "$DEB_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$DEB_DIR/DEBIAN" "$ICON_DIR" "$DESKTOP_DIR"

# Copy executable
if [[ -f "${BUILD_DIR}/${APP_NAME}" ]]; then
  cp "${BUILD_DIR}/${APP_NAME}" "${BIN_DIR}/"
else
  echo "❌ Could not find built binary in ${BUILD_DIR}"
  exit 1
fi

# -----------------------------------------------------------------------------
# 📦 Add icon and .desktop entry
# -----------------------------------------------------------------------------
echo "=== 🎨 Adding icon and desktop entry ==="

# Copy app icon (fallback if not found)
ICON_SRC="icons/application.png"
if [[ -f "$ICON_SRC" ]]; then
  cp "$ICON_SRC" "${ICON_DIR}/${APP_NAME}.png"
else
  echo "  ⚠️ Icon not found at $ICON_SRC — creating placeholder"
  convert -size 256x256 xc:lightgray -gravity center \
    -pointsize 20 -annotate 0 "${APP_NAME}" "${ICON_DIR}/${APP_NAME}.png" || true
fi

# Create .desktop launcher
cat > "${DESKTOP_DIR}/${APP_NAME}.desktop" <<EOF
[Desktop Entry]
Name=${APP_NAME}
Exec=${APP_NAME}
Icon=${APP_NAME}
Type=Application
Categories=Utility;Science;
EOF

# -----------------------------------------------------------------------------
# 🔍 Collect dependent libraries
# -----------------------------------------------------------------------------
echo "=== 🔍 Collecting dependent libraries ==="
TMP_ALL=$(mktemp)
trap 'rm -f "$TMP_ALL" "${TMP_ALL}.uniq" "${TMP_ALL}.final"' EXIT

collect_ldd() {
  local f="$1"
  [[ -e "$f" ]] || return
  ldd "$f" 2>/dev/null \
    | awk '/=>/ { if ($3 ~ /^\//) print $3 } /^\/|^\t\// { if ($1 ~ /^\//) print $1 }' \
    >> "$TMP_ALL" || true
}

for exe in "${BIN_DIR}"/*; do
  [[ -f "$exe" ]] && collect_ldd "$exe"
done

# Add Qt & VTK libs known to be required
ldconfig -p | awk '/libQt5(Core|Gui|Widgets)/ {print $4}' | sort -u >> "$TMP_ALL" || true
ldconfig -p | awk '/libvtk.*9[.]1/ {print $4}' | sort -u >> "$TMP_ALL" || true

# include libraries needed by Qt platform plugins (xcb etc.)
QT_PLUGIN_SRC="/usr/lib/x86_64-linux-gnu/qt5/plugins"
if [[ -d "$QT_PLUGIN_SRC" ]]; then
  find "$QT_PLUGIN_SRC" -type f -name '*.so' -print0 | \
    xargs -0 -r -n1 bash -c 'ldd "$0" 2>/dev/null | awk "/=>/ { if (\$3 ~ /^\\//) print \$3 }"' {} \; \
    >> "$TMP_ALL" || true
fi

# --- Add Qt/XCB extra dependencies explicitly ---
echo "=== 🧩 Adding extra Qt/XCB dependencies ==="
ldconfig -p | awk '/libQt5XcbQpa\.so/ {print $4}' >> "$TMP_ALL" || true
ldconfig -p | awk '/libxcb-(icccm|image|keysyms|render-util|shm|xinerama|xinput|xfixes|sync|cursor)\.so/ {print $4}' >> "$TMP_ALL" || true
ldconfig -p | awk '/libxkbcommon(-x11)?\.so/ {print $4}' >> "$TMP_ALL" || true
ldconfig -p | awk '/libX11\.so/ {print $4}' >> "$TMP_ALL" || true

sort -u "$TMP_ALL" > "${TMP_ALL}.uniq"

# Filter out system/core libs
grep -vE '/lib64/ld-linux|/lib/x86_64-linux-gnu/(libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libnss|libresolv|libcrypt|libutil|libaudit|libsystemd)' "${TMP_ALL}.uniq" \
  | grep -vE '(/usr/lib/llvm|/usr/lib/gcc|/usr/lib/x86_64-linux-gnu/mesa)' \
  > "${TMP_ALL}.final"

# -----------------------------------------------------------------------------
# 📦 Copy collected libraries (with proper symlink target resolution)
# -----------------------------------------------------------------------------
echo "=== 📦 Copying libraries to ${LIB_DIR} ==="
COUNT=0

# Open file as stdin without subshell
exec 9< "${TMP_ALL}.final"

while IFS= read -r libpath <&9; do
  [[ -z "$libpath" || ! -e "$libpath" ]] && continue

  COUNT=$((COUNT + 1))
  base=$(basename "$libpath")

  # Display progress with carriage return (overwrite previous line)
  printf "\r  Copying library %d: %-60s" "$COUNT" "$base"

  # Copy both symlink and its real file
  if [[ -L "$libpath" ]]; then
    cp -a "$libpath" "${LIB_DIR}/"  # copy symlink
    real=$(readlink -f "$libpath" || true)
    if [[ -n "$real" && -e "$real" ]]; then
      rbase=$(basename "$real")
      [[ ! -f "${LIB_DIR}/${rbase}" ]] && cp -a "$real" "${LIB_DIR}/"
    else
      printf "\n  ⚠ Broken symlink: $libpath\n"
    fi
  else
    cp -a "$libpath" "${LIB_DIR}/"
  fi
done

exec 9<&-  # close file descriptor

# Print final summary on new line
printf "\n=== ✅ Copied $COUNT libraries to ${LIB_DIR} ===\n"

# -----------------------------------------------------------------------------
# 🧩 Bundle Qt plugins
# -----------------------------------------------------------------------------
echo "=== 🧩 Bundling Qt platform plugins ==="
QT_PLUGIN_DST="${LIB_DIR}/plugins"
if [[ -d "$QT_PLUGIN_SRC" ]]; then
  mkdir -p "$QT_PLUGIN_DST"
  rsync -a --prune-empty-dirs --include '*/' \
    --include 'platforms/*.so' \
    --include 'imageformats/*.so' \
    --include 'iconengines/*.so' \
    --exclude '*' \
    "$QT_PLUGIN_SRC/" "$QT_PLUGIN_DST/"
else
  echo "  ⚠️ Qt plugin source not found: $QT_PLUGIN_SRC"
fi

# -----------------------------------------------------------------------------
# 🧩 Ensure all dependencies for Qt xcb platform plugin are bundled
# -----------------------------------------------------------------------------
echo "=== 🧩 Ensuring Qt XCB plugin dependencies are included ==="
XCB_PLUGIN="${LIB_DIR}/plugins/platforms/libqxcb.so"

if [[ -f "$XCB_PLUGIN" ]]; then
  echo "  → Inspecting dependencies of $(basename "$XCB_PLUGIN")"
  TMP_XCB_DEPS=$(mktemp)

  # Collect direct dependencies from ldd
  ldd "$XCB_PLUGIN" 2>/dev/null | awk '/=>/ {if ($3 ~ /^\//) print $3}' > "$TMP_XCB_DEPS"

  # Add known required libraries (even if not directly reported by ldd)
  for lib in \
    libQt5DBus.so.5 libxcb-icccm.so.4 libxcb-image.so.0 libxcb-keysyms.so.1 \
    libxcb-render-util.so.0 libxcb-xinerama.so.0 libxcb-xinput.so.0 \
    libxcb-xfixes.so.0 libxcb-cursor.so.0 libxkbcommon-x11.so.0 \
    libxkbcommon.so.0 libxcb-util.so.1 libX11-xcb.so.1 libX11.so.6 \
    libxcb-render.so.0 libxcb-shm.so.0 libxcb-sync.so.1 libxcb.so.1; do
      # Use || true to prevent grep from failing with set -e
      found=$(ldconfig -p | grep "$lib" | awk '{print $4}' | head -n1 || true)
      if [[ -n "$found" ]]; then
        echo "$found" >> "$TMP_XCB_DEPS"
      else
        echo "  ⚠️  Library not found in ldconfig cache: $lib"
      fi
  done

  # Copy all collected dependencies (including real files behind symlinks)
  COPIED=0
  while IFS= read -r dep; do
    [[ -z "$dep" || ! -e "$dep" ]] && continue
    base=$(basename "$dep")

    # Skip if already copied
    if [[ -e "${LIB_DIR}/${base}" ]]; then
      continue
    fi

    echo "    ↳ Bundling ${base}"

    # Handle symlinks properly - copy both symlink and target
    if [[ -L "$dep" ]]; then
      # Copy the symlink itself
      cp -a "$dep" "${LIB_DIR}/"

      # Find and copy the real file
      real=$(readlink -f "$dep" || true)
      if [[ -n "$real" && -e "$real" ]]; then
        rbase=$(basename "$real")
        if [[ ! -f "${LIB_DIR}/${rbase}" ]]; then
          echo "    ↳ Bundling real file ${rbase}"
          cp -a "$real" "${LIB_DIR}/"
          COPIED=$((COPIED + 1))
        fi
      fi
    else
      # Regular file
      cp -a "$dep" "${LIB_DIR}/"
    fi

    COPIED=$((COPIED + 1))
  done < <(sort -u "$TMP_XCB_DEPS")

  echo "  ✅ Added $COPIED XCB-related libraries"
  rm -f "$TMP_XCB_DEPS"
else
  echo "  ⚠️  No xcb plugin found at $XCB_PLUGIN"
fi

# -----------------------------------------------------------------------------
# 🔍 Recursively collect all dependencies of bundled libraries
# -----------------------------------------------------------------------------
echo "=== 🔍 Recursively collecting transitive dependencies ==="
MAX_ITERATIONS=5
ITERATION=0

while [[ $ITERATION -lt $MAX_ITERATIONS ]]; do
  ITERATION=$((ITERATION + 1))
  echo "  → Iteration $ITERATION..."

  TMP_NEW_DEPS=$(mktemp)
  FOUND_NEW=0

  # Check all .so files in LIB_DIR for missing dependencies
  find "${LIB_DIR}" -type f -name '*.so*' | while read -r lib; do
    ldd "$lib" 2>/dev/null | awk '/=>/ {if ($3 ~ /^\//) print $3}' || true
  done | sort -u > "$TMP_NEW_DEPS"

  # Copy any new dependencies found
  while IFS= read -r dep; do
    [[ -z "$dep" || ! -e "$dep" ]] && continue

    # Skip system libraries
    if echo "$dep" | grep -qE '/lib64/ld-linux|/lib/x86_64-linux-gnu/(libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libnss|libresolv|libcrypt|libutil|libaudit|libsystemd)'; then
      continue
    fi

    base=$(basename "$dep")

    # Check if already exists
    if [[ ! -e "${LIB_DIR}/${base}" ]]; then
      FOUND_NEW=$((FOUND_NEW + 1))
      printf "\r    Found new dependency: %-60s" "$base"

      if [[ -L "$dep" ]]; then
        cp -a "$dep" "${LIB_DIR}/"
        real=$(readlink -f "$dep" || true)
        if [[ -n "$real" && -e "$real" ]]; then
          rbase=$(basename "$real")
          [[ ! -f "${LIB_DIR}/${rbase}" ]] && cp -a "$real" "${LIB_DIR}/"
        fi
      else
        cp -a "$dep" "${LIB_DIR}/"
      fi
    fi
  done < "$TMP_NEW_DEPS"

  rm -f "$TMP_NEW_DEPS"

  if [[ $FOUND_NEW -eq 0 ]]; then
    printf "\r  ✅ No new dependencies found - all resolved!%60s\n" ""
    break
  else
    printf "\r  → Found and copied %d new dependencies%60s\n" "$FOUND_NEW" ""
  fi
done

# -----------------------------------------------------------------------------
# 🔧 Normalize symlinks
# -----------------------------------------------------------------------------
echo "=== 🔧 Normalizing symlinks inside ${LIB_DIR} ==="
cd "${LIB_DIR}"
for ln in ./*; do
  [[ -L "$ln" ]] || continue
  dest=$(readlink "$ln")
  dest_base=$(basename "$dest")
  if [[ "$dest" != "$dest_base" ]]; then
    rm -f "$ln"
    ln -s "$dest_base" "$ln"
  fi
done
cd - >/dev/null

# -----------------------------------------------------------------------------
# 🧭 Patch RPATH/RUNPATH inside libraries (selectively)
# -----------------------------------------------------------------------------
# WHY THIS IS NEEDED:
# -------------------
# Many shared libraries (especially those from Qt, VTK, or OpenMPI/Hwloc)
# are built with embedded RPATH or RUNPATH entries that point to absolute
# system locations — e.g. /usr/lib/x86_64-linux-gnu, /lib, or build directories.
#
# When we copy these .so files into our own package directory
# (/usr/lib/OOpenCal-Viewer), the dynamic linker (ld-linux) will *still*
# try to resolve dependencies using those old system paths. This leads to:
#   - "not found" errors at runtime (e.g. libopen-rte.so.40 => not found)
#   - accidental mixing of system and bundled libraries
#   - missing symbols or ABI mismatches
#
# Example issue observed during development:
#   ldd /usr/bin/OOpenCal-Viewer | grep not
#       libopen-rte.so.40 => not found
#       libopen-pal.so.40 => not found
#       libhwloc.so.15 => not found
#
# Those libraries *were* copied into /usr/lib/OOpenCal-Viewer,
# but the RPATH of some dependent libs (e.g. VTK or Qt plugins)
# pointed to system locations instead of the local directory.
#
# HOW WE FIX IT:
# --------------
# We inspect each .so file inside our packaged lib directory.
# If it contains RPATH or RUNPATH, we forcibly replace it with:
#     $ORIGIN:/usr/lib/OOpenCal-Viewer
#
# "$ORIGIN" expands at runtime to the directory of the current library file,
# so this allows the linker to find dependencies relative to the package.
# We also include /usr/lib/OOpenCal-Viewer as a fallback path,
# in case the loader resolves from the executable's RPATH.
#
# Without this fix, even though all dependencies are copied,
# OOpenCal-Viewer might still crash on a clean system (no dev packages)
# because it tries to link against non-existent system libraries.
#
# This section was one of the hardest to diagnose during development —
# the symptom looked like "missing libraries" but the cause was stale
# RPATH entries pointing outside the package.
# -----------------------------------------------------------------------------
echo "=== 🧭 Auditing and patching RPATH/RUNPATH in bundled libs ==="

find "${LIB_DIR}" -type f -name '*.so*' | while read -r so; do
  # Only inspect ELF shared libraries that contain RPATH or RUNPATH entries.
  if readelf -d "$so" 2>/dev/null | grep -Eq 'RUNPATH|RPATH'; then
    current=$(readelf -d "$so" 2>/dev/null | grep -E 'RUNPATH|RPATH' | sed -E 's/.*Library (rpath|runpath): \[(.*)\].*/\2/')
    echo "  ⚙️  Found RPATH in $(basename "$so"): ${current}"
    echo "     → Replacing with: \$ORIGIN:/usr/lib/${APP_NAME}"

    # Replace any existing RPATH/RUNPATH with a safe relative path.
    # Note: --force-rpath ensures RUNPATH is replaced if needed.
    patchelf --force-rpath --set-rpath '$ORIGIN:/usr/lib/'"${APP_NAME}" "$so" || true
  fi
done

# Also patch Qt plugins
echo "=== 🧭 Patching RPATH for Qt plugins ==="
find "${LIB_DIR}/plugins" -type f -name '*.so' 2>/dev/null | while read -r plugin; do
  patchelf --force-rpath --set-rpath '$ORIGIN/../..:/usr/lib/'"${APP_NAME}" "$plugin" || true
done

echo "=== 🧭 Setting RPATH on executables ==="
for exe in "${BIN_DIR}"/*; do
  [[ -f "$exe" ]] && file "$exe" | grep -q ELF && \
  patchelf --force-rpath --set-rpath '$ORIGIN/../lib/'"${APP_NAME}" "$exe" || true
done

# -----------------------------------------------------------------------------
# 🗒 Create DEBIAN/control
# -----------------------------------------------------------------------------
echo "=== 🗒 Creating DEBIAN/control ==="
VERSION="1.0.$(date +%s)"
cat > "${DEB_DIR}/DEBIAN/control" <<EOF
Package: ${APP_NAME}
Version: ${VERSION}
Section: science
Priority: optional
Architecture: amd64
Maintainer: Your Name <you@example.com>
Description: ${APP_NAME} with bundled Qt + VTK + OpenMPI/Hwloc (self-contained)
EOF

chmod 0755 "${DEB_DIR}/DEBIAN"
chmod -R 0755 "${BIN_DIR}" || true

# -----------------------------------------------------------------------------
# 📦 Build the .deb package
# -----------------------------------------------------------------------------
echo "=== 📦 Building Debian package ==="
dpkg-deb --build "${DEB_DIR}" "${DEB_FILE}"

# Get human-readable file size
FILE_SIZE=$(du -h "${DEB_FILE}" | awk '{print $1}')
echo
echo "=== ✅ Done! Created: ${DEB_FILE} (${FILE_SIZE}) ==="
echo "To install:   sudo apt install ./$(basename "${DEB_FILE}")"
echo "To uninstall: sudo dpkg -r ${APP_NAME}"
echo "To purge:     sudo dpkg -P ${APP_NAME}"
echo "To inspect:   dpkg-deb -c ${DEB_FILE}"
echo
