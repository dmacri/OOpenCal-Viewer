#!/usr/bin/env bash
set -e

# ============================================================
# Resolve viewer repository root directory (regardless of where
# the script is invoked from).
# ============================================================
SCRIPT_PATH="$(realpath "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
VIEWER_ROOT="$(realpath "$SCRIPT_DIR/..")"

# ============================================================
# Configuration
# ============================================================
VCPKG_DIR="${VIEWER_ROOT}/vcpkg"
BUILD_DIR="${VIEWER_ROOT}/build"
TRIPLET="x64-linux"   # Change if needed (e.g. x64-windows)
VCPKG_URL="https://github.com/microsoft/vcpkg.git"

# ============================================================
# Parse arguments
# ============================================================
OOPENCAL_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --oopencal)
            OOPENCAL_DIR="$(realpath "$2")"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 --oopencal <path>"
            exit 1
            ;;
    esac
done

if [ -z "$OOPENCAL_DIR" ]; then
    echo "ERROR: Missing parameter --oopencal <path>"
    exit 1
fi

# ============================================================
# Validate OOpenCal directory
# ============================================================
if [ ! -d "$OOPENCAL_DIR" ]; then
    echo "ERROR: Provided OOpenCAL directory does not exist:"
    echo "  $OOPENCAL_DIR"
    exit 1
fi

if [ ! -d "$OOPENCAL_DIR/OOpenCAL" ]; then
    echo "ERROR: Provided directory does not contain 'OOpenCAL' folder:"
    echo "  $OOPENCAL_DIR/OOpenCAL"
    exit 1
fi

echo "=========================================="
echo " OOpenCAL detected at: $OOPENCAL_DIR"
echo " Viewer root directory: $VIEWER_ROOT"
echo "=========================================="

# ============================================================
# Clone vcpkg if missing
# ============================================================
if [ ! -d "$VCPKG_DIR" ]; then
    echo "[1/5] Cloning vcpkg..."
    git clone --depth=1 "$VCPKG_URL" "$VCPKG_DIR"
else
    echo "[1/5] vcpkg already present"
fi

# ============================================================
# Bootstrap vcpkg
# ============================================================
echo "[2/5] Bootstrapping vcpkg..."
"$VCPKG_DIR/bootstrap-vcpkg.sh --disable-metrics"

# ============================================================
# Install dependencies
# ============================================================
echo "[3/5] Installing dependencies using vcpkg..."

"$VCPKG_DIR/vcpkg" install "vtk[qt,opengl]" --triplet="$TRIPLET"
"$VCPKG_DIR/vcpkg" install "qt6-base" --triplet="$TRIPLET"

# ============================================================
# Configure build
# ============================================================
echo "[4/5] Configuring CMake..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$VIEWER_ROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOOPENCAL_DIR="$OOPENCAL_DIR"

# ============================================================
# Build
# ============================================================
echo "[5/5] Building project..."
cmake --build . --config Release

echo "=========================================="
echo " Build complete!"
echo " Executable should be inside:"
echo "   $BUILD_DIR"
echo "=========================================="
