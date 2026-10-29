#!/usr/bin/env bash
# Copies everything a model plugin needs *at compile time* (compiled on the user's machine by the bundled compiler) into a package:
#
#   <dest>/include/oopencal-viewer/   headers of the Viewer (what OOPENCAL_VIEWER_ROOT points to)
#   <dest>/include/OOpenCAL/base/     headers of OOpenCAL (what OOPENCAL_DIR points to is <dest>/include)
#   <dest>/include/vtk/               headers of VTK (what VTK_INCLUDES points to)
#   <dest>/compiler/include/...       third party headers which the VTK/Viewer headers include (e.g. fmt), found without any -I
#
# The third party headers are found by really preprocessing a model wrapper (smoke-test/SmokePlugin.cpp) instead of guessing.
#
# Usage: package-headers.sh <viewer-repo> <oopencal-parent-dir> <vtk-include-dir> <system-include-dir> <dest> [compiler]
#   <oopencal-parent-dir>  directory which contains OOpenCAL/ (OOPENCAL_DIR of the CMake build)
#   <system-include-dir>   include directory of the toolchain prefix (MinGW: $MINGW_PREFIX/include)
set -euo pipefail

if [[ $# -lt 5 ]]; then
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

REPO=$(cd "$1" && pwd)
OOPENCAL_PARENT=$(cd "$2" && pwd)
VTK_INCLUDE=$(cd "$3" && pwd)
SYSTEM_INCLUDE=$(cd "$4" && pwd)
DEST=$5
CXX=${6:-g++}

[[ -d "$OOPENCAL_PARENT/OOpenCAL/base" ]] || { echo "error: $OOPENCAL_PARENT/OOpenCAL/base not found" >&2; exit 1; }

mkdir -p "$DEST/include/oopencal-viewer" "$DEST/include/OOpenCAL" "$DEST/include/vtk" "$DEST/compiler/include"

echo "== Viewer headers"
# Headers only, directory structure kept. widgets/ is needed: visualiser/Visualizer.hpp includes widgets/GridWorldMapping.h
( cd "$REPO" && find config core data plugins visualiser visualiserProxy widgets \
      \( -name '*.h' -o -name '*.hpp' \) -type f -print0 \
    | xargs -0 cp --parents -t "$DEST/include/oopencal-viewer" )

echo "== OOpenCAL headers"
cp -r "$OOPENCAL_PARENT/OOpenCAL/base" "$DEST/include/OOpenCAL/"

echo "== VTK headers"
cp -r "$VTK_INCLUDE/." "$DEST/include/vtk/"

echo "== Third party headers included by the above (found by preprocessing a model wrapper)"
probe_dir=$(mktemp -d)
trap 'rm -rf "$probe_dir"' EXIT
cp "$REPO/scripts/windows-mingw/smoke-test/SmokePlugin.cpp" "$REPO/examples/custom_model_plugin/CustomCell.h" "$probe_dir/"

# Exactly the include directories which CompilationConfig gives to the compiler, but pointing into the package
"$CXX" -std=c++23 -DBUILDING_DLL -M \
    -I"$DEST/include" -I"$DEST/include/OOpenCAL" -I"$DEST/include/oopencal-viewer" \
    -I"$DEST/include/oopencal-viewer/visualiserProxy" -I"$DEST/include/oopencal-viewer/config" \
    -I"$DEST/include/vtk" "$probe_dir/SmokePlugin.cpp" > "$probe_dir/deps.txt"

# The compiler may print Windows paths (C:/msys64/ucrt64/include/x.h): bring them to the form of $SYSTEM_INCLUDE
tr -s ' \\' '\n\n' < "$probe_dir/deps.txt" | grep -E '\.(h|hpp|hh|inc|tcc)$' | grep -v '^$' | sort -u > "$probe_dir/headers.txt"
if command -v cygpath > /dev/null; then
    cygpath -u -f "$probe_dir/headers.txt" > "$probe_dir/headers-posix.txt"
    mv "$probe_dir/headers-posix.txt" "$probe_dir/headers.txt"
fi

copied=0
while IFS= read -r header; do
    # only headers of the toolchain prefix (/ucrt64/include/...), which are not the ones already in the package
    [[ "$header" == "$SYSTEM_INCLUDE"/* ]] || continue
    rel=${header#"$SYSTEM_INCLUDE"/}
    [[ "$rel" == c++/* ]] && continue    # libstdc++ belongs to the compiler itself
    target="$DEST/compiler/include/$rel"
    [[ -e "$target" ]] && continue
    mkdir -p "$(dirname "$target")"
    cp "$header" "$target"
    copied=$((copied + 1))
done < "$probe_dir/headers.txt"
echo "   copied $copied third party header(s) into compiler/include"

echo "== Done: $(du -sh "$DEST/include" | cut -f1) in include/"
