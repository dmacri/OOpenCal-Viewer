#!/usr/bin/env bash
set -e

# ================================================================
# Resolve script directory
# ================================================================
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
    SOURCE="$( readlink "$SOURCE" )"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
VIEWER_ROOT="$(realpath "$SCRIPT_DIR/..")"

echo "============================================"
echo "   OOpenCal-Viewer: Dependency & Build Tool  "
echo "============================================"
echo "SCRIPT_DIR  = $SCRIPT_DIR"
echo "VIEWER_ROOT = $VIEWER_ROOT"
echo

# ================================================================
# Parse arguments
# ================================================================
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

if [[ -z "$OOPENCAL_DIR" ]]; then
    echo "ERROR: Missing required parameter --oopencal <path>"
    exit 1
fi

# ================================================================
# Validate OOpenCAL directory
# ================================================================
if [[ ! -d "$OOPENCAL_DIR" ]]; then
    echo "ERROR: Provided OOpenCAL directory does not exist:"
    echo "  $OOPENCAL_DIR"
    exit 1
fi

if [[ ! -d "$OOPENCAL_DIR/OOpenCAL" ]]; then
    echo "ERROR: Provided directory does not contain 'OOpenCAL' folder:"
    echo "  $OOPENCAL_DIR/OOpenCAL"
    exit 1
fi

echo "[INFO] OOpenCAL found at: $OOPENCAL_DIR"
echo

# ================================================================
# Detect OS
# ================================================================
OS="unknown"
if [[ -f /etc/os-release ]]; then
    source /etc/os-release
    OS="$ID"
fi

echo "[INFO] Detected OS: $OS"
echo

# ================================================================
# Debian/Ubuntu: check required packages
# ================================================================
if [[ "$OS" == "ubuntu" || "$OS" == "debian" ]]; then
    echo "[INFO] Debian/Ubuntu detected → checking packages."

    REQUIRED_PACKAGES=(
        build-essential
        cmake
        qtbase5-dev
        qttools5-dev
        qttools5-dev-tools
        libvtk9-dev
        libvtk9-qt-dev
        libfreetype-dev
        libeigen3-dev
        libtiff-dev
        libpng-dev
        libjpeg-dev
        zlib1g-dev
        libgl1-mesa-dev
        libglu1-mesa-dev
    )

    MISSING=()
    for pkg in "${REQUIRED_PACKAGES[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            MISSING+=("$pkg")
        fi
    done

    if [[ ${#MISSING[@]} -gt 0 ]]; then
        echo
        echo "=============================================="
        echo " MISSING SYSTEM PACKAGES"
        echo " The following packages are required:"
        echo
        printf "  - %s\n" "${MISSING[@]}"
        echo
        echo "Install them using:"
        echo "  sudo apt update && sudo apt install -y ${MISSING[*]}"
        echo "=============================================="
        echo
        echo "ERROR: Missing dependencies → build cannot continue."
        exit 1
    else
        echo "[OK] All required APT packages installed."
    fi
else
    # Non-Debian systems
    echo "=============================================================="
    echo " NON-DEBIAN LINUX DETECTED"
    echo " This script does NOT install packages automatically."
    echo " You must ensure the following are installed on your system:"
    echo
    echo "  - Qt5 development packages (qmake, qtbase5-dev, qttools)"
    echo "  - VTK (with Qt support enabled)"
    echo "  - CMake"
    echo "  - GCC / Clang toolchain"
    echo
    echo "If these are not installed, the build will fail."
    echo "=============================================================="
    echo
fi

# ================================================================
# Build Viewer using CMake (Makefiles only)
# ================================================================
BUILD_DIR="${VIEWER_ROOT}/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Detect ninja but DO NOT use it (your request)
if command -v ninja >/dev/null 2>&1; then
    echo "[INFO] Ninja is installed, but will NOT be used (Makefile mode forced)."
fi

echo "[BUILD] Configuring CMake (Makefiles)..."
cmake "$VIEWER_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    -DOOPENCAL_DIR="$OOPENCAL_DIR"

echo "[BUILD] Building using make..."
make -j"$(nproc)"

echo
echo "=========================================="
echo " Build completed successfully!"
echo " Viewer binary is in:"
echo "   $BUILD_DIR"
echo "=========================================="
