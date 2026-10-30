#!/usr/bin/env bash
# Builds the portable Windows package of OOpenCal-Viewer: the program built with MinGW-w64 (UCRT) together with the very same
# C++ toolchain, so that models can be compiled on the user's machine without installing anything.
#
# It has to be run inside an MSYS2 UCRT64 shell, after OOpenCal-Viewer has been built with that toolchain
# (see .github/workflows/windows-mingw-all-in-one.yml, where it is used, and doc/WINDOWS_MINGW_PORTABLE.md).
#
# Usage: package.sh <viewer-repo> <build-dir> <oopencal-parent-dir> <output-dir>
#   <build-dir>            CMake build directory, with OOpenCal-Viewer.exe and libOOpenCal-Viewer.dll.a
#   <oopencal-parent-dir>  directory which contains OOpenCAL/ (OOPENCAL_DIR of the CMake build)
#   <output-dir>           the package is created here (removed first)
#
# Layout of the result (OOpenCal-Viewer.bat is what the user double-clicks):
#   OOpenCal-Viewer.bat, compiler-env.bat, README.txt, THIRD-PARTY-NOTICES.txt
#   bin/       OOpenCal-Viewer.exe, Qt and VTK DLLs, runtime of the compiler, Qt plugins (bin/plugins, found through bin/qt.conf)
#   compiler/  MinGW-w64 g++ with its headers and libraries (prefix layout: bin, lib, include, ...)
#   include/   headers for compiling models: oopencal-viewer/, OOpenCAL/base/, vtk/
#   lib/       libOOpenCal-Viewer.dll.a, import libraries of VTK, plugin-libs.rsp (see compiler-env.bat)
set -euo pipefail

if [[ $# -ne 4 ]]; then
    sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
: "${MINGW_PREFIX:?this script has to be run in an MSYS2 MinGW shell (e.g. msystem UCRT64)}"
: "${MINGW_PACKAGE_PREFIX:?this script has to be run in an MSYS2 MinGW shell (e.g. msystem UCRT64)}"

REPO=$(cd "$1" && pwd)
BUILD=$(cd "$2" && pwd)
OOPENCAL_PARENT=$(cd "$3" && pwd)
PKG=$4
PREFIX=$MINGW_PREFIX                    # e.g. /ucrt64
HERE=$(cd "$(dirname "$0")" && pwd)

EXE="$BUILD/OOpenCal-Viewer.exe"
[[ -f "$EXE" ]] || { echo "error: $EXE not found - build the project first" >&2; exit 1; }
IMPLIB="$BUILD/libOOpenCal-Viewer.dll.a"
if [[ ! -f "$IMPLIB" ]]; then
    IMPLIB=$(find "$BUILD" -maxdepth 3 -name 'libOOpenCal-Viewer.dll.a' | head -n1)
fi
[[ -n "$IMPLIB" && -f "$IMPLIB" ]] || {
    echo "error: libOOpenCal-Viewer.dll.a (import library of the executable) was not produced." >&2
    echo "       Models could not be linked. Is ENABLE_EXPORTS set for MinGW in CMakeLists.txt?" >&2
    exit 1
}

rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/compiler"
PKG=$(cd "$PKG" && pwd)

# DLLs from the MinGW prefix which the given binaries need (recursively), one path per line
dll_closure() {
    local f
    for f in "$@"; do
        ldd "$f" 2>/dev/null || true
    done | awk -v p="$PREFIX/bin/" 'index($3, p) == 1 { print $3 }' | sort -u
}

copy_dlls() { # <destination> <binary>...
    local dest=$1 dll
    shift
    while IFS= read -r dll; do
        cp -n "$dll" "$dest/"
    done < <(dll_closure "$@")
}

# ------------------------------------------------------------------------------------------------------------------------------
echo "== Program"
cp "$EXE" "$PKG/bin/"
copy_dlls "$PKG/bin" "$PKG/bin/OOpenCal-Viewer.exe"

echo "== Qt plugins"
QT_PLUGINS=$(qtpaths6 --plugin-dir 2>/dev/null || true)
[[ -d "$QT_PLUGINS" ]] || QT_PLUGINS="$PREFIX/share/qt6/plugins"
[[ -d "$QT_PLUGINS" ]] || { echo "error: Qt plugins directory not found (looked at $QT_PLUGINS)" >&2; exit 1; }
for dir in platforms styles imageformats iconengines generic networkinformation tls; do
    if [[ -d "$QT_PLUGINS/$dir" ]]; then
        mkdir -p "$PKG/bin/plugins"
        cp -r "$QT_PLUGINS/$dir" "$PKG/bin/plugins/"
    fi
done
# Qt looks for its plugins relative to its own installation prefix, which does not exist in the package: say where they are
# (the same thing which the MSVC package does)
printf '[Paths]\r\nPrefix = .\r\nPlugins = plugins\r\n' > "$PKG/bin/qt.conf"
[[ -f "$PKG/bin/plugins/platforms/qwindows.dll" ]] || { echo "error: qwindows.dll (Qt platform plugin) is missing" >&2; exit 1; }
# the plugins have their own dependencies
mapfile -t plugin_dlls < <(find "$PKG/bin/plugins" -name '*.dll')
copy_dlls "$PKG/bin" "${plugin_dlls[@]}"

# ------------------------------------------------------------------------------------------------------------------------------
echo "== Compiler (MinGW-w64 toolchain)"
# Take the files of the packages which make the toolchain, so nothing depends on guessing paths:
# gcc (g++, cc1plus, libstdc++ headers), binutils (as, ld), the C runtime headers and import libraries, winpthreads
toolchain_packages=$(pacman -Qq | grep -E "^${MINGW_PACKAGE_PREFIX}-(gcc|gcc-libs|binutils|headers(-git)?|crt(-git)?|winpthreads(-git)?|libwinpthread(-git)?|windows-default-manifest)$" || true)
for required in gcc binutils; do
    grep -qx "${MINGW_PACKAGE_PREFIX}-${required}" <<<"$toolchain_packages" || { echo "error: package ${MINGW_PACKAGE_PREFIX}-${required} is not installed" >&2; exit 1; }
done
echo "$toolchain_packages" | sed 's/^/   /'

file_list=$(mktemp)
trap 'rm -f "$file_list"' EXIT
for p in $toolchain_packages; do pacman -Ql "$p"; done \
    | awk '{ print $2 }' \
    | grep -v '/$' \
    | grep -Ev "^$PREFIX/(share|etc|var)/" \
    | grep -Ev '/lib/gcc/[^/]+/[^/]+/(plugin|install-tools|finclude)/' \
    | grep -Ev '/(lto1|f951|cc1obj|cc1objplus|jc1|gnat1)\.exe$' \
    | while IFS= read -r f; do if [[ -f "$f" || -L "$f" ]]; then echo "${f#"$PREFIX"/}"; fi; done > "$file_list" || true
echo "   $(wc -l < "$file_list") files"
tar -C "$PREFIX" -cf - -T "$file_list" | tar -C "$PKG/compiler" -xf -

[[ -f "$PKG/compiler/bin/g++.exe" ]] || { echo "error: g++.exe is not in the package" >&2; exit 1; }
for header in include/wchar.h include/stdio.h include/windows.h; do
    [[ -f "$PKG/compiler/$header" ]] || { echo "error: compiler/$header was not copied from ${MINGW_PACKAGE_PREFIX}-headers" >&2; exit 1; }
done
compgen -G "$PKG/compiler/include/c++/*/iostream" > /dev/null || { echo "error: the libstdc++ headers were not copied" >&2; exit 1; }
mapfile -t compiler_binaries < <(find "$PKG/compiler/bin" "$PKG/compiler/lib/gcc" -name '*.exe' 2>/dev/null)
copy_dlls "$PKG/compiler/bin" "${compiler_binaries[@]}"

# Where does the relocated compiler look for headers? (shown to make a failure below easy to understand)
echo "   include search path of the packaged g++:"
"$PKG/compiler/bin/g++.exe" -x c++ -E -v - < /dev/null 2>&1 > /dev/null | sed -n '/search starts here/,/End of search list/p' | sed 's/^/   | /' || true

# ------------------------------------------------------------------------------------------------------------------------------
echo "== Headers for compiling models"
# MSYS2 installs the headers of VTK to include/vtk (older packaging and other distributions: include/vtk-<version>)
VTK_INCLUDE=$(find "$PREFIX/include" -maxdepth 1 -type d \( -name 'vtk' -o -name 'vtk-*' \) | sort -V | tail -n1)
[[ -n "$VTK_INCLUDE" ]] || { echo "error: VTK headers not found in $PREFIX/include" >&2; exit 1; }
# The headers which a model needs are found with the compiler of the build environment, whose include path is known to be right
# (the packaged one is tested right below and by the smoke test)
"$HERE/package-headers.sh" "$REPO" "$OOPENCAL_PARENT" "$VTK_INCLUDE" "$PREFIX/include" "$PKG" g++

echo "== The packaged compiler preprocesses a model wrapper (with the flags of compiler-env.bat)"
if ! "$PKG/compiler/bin/g++.exe" -std=c++23 -DBUILDING_DLL -fsyntax-only -idirafter "$PKG/compiler/include" \
        -I"$PKG/include" -I"$PKG/include/OOpenCAL" -I"$PKG/include/oopencal-viewer" \
        -I"$PKG/include/oopencal-viewer/visualiserProxy" -I"$PKG/include/oopencal-viewer/config" -I"$PKG/include/vtk" \
        "$HERE/smoke-test/SmokePlugin.cpp" -I"$REPO/examples/custom_model_plugin" 2> "$file_list.syntax"; then
    head -n 40 "$file_list.syntax"
    echo "error: the packaged g++ cannot compile a model wrapper - see above (and the include search path printed earlier)" >&2
    rm -f "$file_list.syntax"
    exit 1
fi
rm -f "$file_list.syntax"
echo "   ok"

# ------------------------------------------------------------------------------------------------------------------------------
echo "== Libraries which models are linked with"
# A Windows DLL has to be linked with everything it uses: the import library of the executable (models call back into it)
# and the import libraries of the VTK DLLs which are shipped. Linking with a library which a model does not use costs nothing.
cp "$IMPLIB" "$PKG/lib/libOOpenCal-Viewer.dll.a"
: > "$PKG/lib/plugin-libs.rsp"
for dll in "$PKG"/bin/libvtk*.dll; do
    name=$(basename "$dll" .dll)                        # libvtkCommonCore
    if [[ -f "$PREFIX/lib/$name.dll.a" ]]; then
        cp "$PREFIX/lib/$name.dll.a" "$PKG/lib/"
        echo "-l${name#lib}" >> "$PKG/lib/plugin-libs.rsp"
    else
        echo "   warning: no import library for $name.dll"
    fi
done
echo "   $(wc -l < "$PKG/lib/plugin-libs.rsp") VTK import libraries"

# ------------------------------------------------------------------------------------------------------------------------------
echo "== Launcher and documents"
cp "$HERE/OOpenCal-Viewer.bat" "$HERE/compiler-env.bat" "$PKG/"
[[ -f "$REPO/LICENSE" ]] && cp "$REPO/LICENSE" "$PKG/LICENSE-OOpenCal-Viewer.txt"

cat > "$PKG/README.txt" <<'EOF'
OOpenCal-Viewer - portable Windows version with a built-in C++ compiler
=======================================================================

1. Extract the whole ZIP archive to a directory WITHOUT spaces in its path (e.g. C:\OOpenCal-Viewer).
2. Double-click OOpenCal-Viewer.bat.
3. Open a model directory: the C++ model is compiled automatically by the compiler from the "compiler" directory.

Nothing has to be installed (no Visual Studio, no MinGW, no administrator rights). Command line arguments are passed to the
program, e.g.:  OOpenCal-Viewer.bat C:\path\to\model\directory

The compiler is MinGW-w64 (GCC, UCRT runtime). The program, Qt, VTK and the models are all built with it, so they are
compatible with each other. Do not replace the compiler with another one (e.g. Visual Studio): C++ code built by different
compilers cannot be mixed.

Layout:  bin\ program and its DLLs | compiler\ g++ | include\ headers for models | lib\ libraries for models
Licenses of the bundled components: THIRD-PARTY-NOTICES.txt
EOF

{
    echo "Third party components bundled in this package"
    echo "=============================================="
    echo
    echo "The compiler, its runtime, Qt, VTK and the libraries they need come unmodified from the MSYS2 project"
    echo "(https://www.msys2.org, packages ${MINGW_PACKAGE_PREFIX}-*). They are distributed under their own licenses"
    echo "(GCC and binutils: GPLv3 with the GCC Runtime Library Exception; Qt: LGPLv3; VTK: BSD-3-Clause; others see below)."
    echo "The code which the compiler builds from the user's models is NOT covered by the GPL (GCC Runtime Library Exception)."
    echo "Complete corresponding source code of every package: https://github.com/msys2/MINGW-packages (PKGBUILD of the package)"
    echo "and the upstream project of the package; versions are listed here."
    echo
    echo "Package, version - license:"
    # the toolchain packages, and the packages which own the DLLs shipped in bin/ and compiler/bin/ (Qt, VTK, their dependencies)
    owned_files=()
    for f in "$PKG"/bin/*.dll "$PKG"/compiler/bin/*.dll; do
        [[ -f "$PREFIX/bin/$(basename "$f")" ]] && owned_files+=("$PREFIX/bin/$(basename "$f")")
    done
    {
        printf '%s\n' $toolchain_packages
        pacman -Qoq "${owned_files[@]}" 2>/dev/null || true
        pacman -Qoq "$QT_PLUGINS/platforms/qwindows.dll" 2>/dev/null || true
    } | sort -u > "$file_list.packages"
    # shellcheck disable=SC2046
    pacman -Qi $(cat "$file_list.packages") | awk -F' *: *' '/^Name/ { n = $2 } /^Version/ { v = $2 } /^Licenses/ { print "  " n " " v " - " $2 }'
    rm -f "$file_list.packages"
} > "$PKG/THIRD-PARTY-NOTICES.txt"

echo "== Package ready: $PKG ($(du -sh "$PKG" | cut -f1))"
