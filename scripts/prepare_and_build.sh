#!/usr/bin/env bash
set -e

# ================================================================
#  OOpenCal Viewer - Build Script
# ================================================================
#
#  NAME:
#      build_viewer.sh
#
#  DESCRIPTION:
#      This script builds the OOpenCal-Viewer project.
#      It automatically:
#         - Detects OS (Ubuntu/Debian or Other Linux)
#         - Checks required system packages on Debian-based systems
#         - Uses CMake + Make (not Ninja)
#         - Supports Qt5 (default) or Qt6 via --qt=6
#         - Validates the OOpenCAL directory
#         - Shows colored logs and build progress
#
#  USAGE:
#      ./build_viewer.sh --oopencal <path> [--qt=VERSION]
#
#  OPTIONS:
#      --oopencal <path>    Path to directory containing OOpenCAL/
#      --qt=VERSION         Qt version: 5 (default) or 6
#
# ================================================================


# ----------------------- COLORS ---------------------------------
RED="\e[31m"
GREEN="\e[32m"
YELLOW="\e[33m"
BLUE="\e[34m"
MAGENTA="\e[35m"
CYAN="\e[36m"
BOLD="\e[1m"
RESET="\e[0m"

log_info()    { echo -e "${CYAN}[INFO]${RESET} $1"; }
log_ok()      { echo -e "${GREEN}[OK]${RESET} $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${RESET} $1"; }
log_err()     { echo -e "${RED}[ERROR]${RESET} $1"; }
log_step()    { echo -e "${MAGENTA}${BOLD}==>${RESET} ${BOLD}$1${RESET}"; }


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

echo -e "${BOLD}============================================"
echo -e "       OOpenCal-Viewer: Build Tool"
echo -e "============================================${RESET}"
echo "SCRIPT_DIR  = $SCRIPT_DIR"
echo "VIEWER_ROOT = $VIEWER_ROOT"
echo


# ================================================================
# Parse arguments
# ================================================================
QT_VERSION="qt5"
OOPENCAL_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --oopencal)
            OOPENCAL_DIR="$(realpath "$2")"
            shift 2
            ;;
        --qt=*)
            QT_REQUESTED="${1#--qt=}"
            if [[ "$QT_REQUESTED" == "5" ]]; then
                QT_VERSION="qt5"
            elif [[ "$QT_REQUESTED" == "6" ]]; then
                QT_VERSION="qt6"
            else
                log_err "Invalid Qt version: $QT_REQUESTED (must be 5 or 6)"
                exit 1
            fi
            shift
            ;;
        *)
            log_err "Unknown argument: $1"
            exit 1
            ;;
    esac
done

if [[ -z "$OOPENCAL_DIR" ]]; then
    log_err "Missing --oopencal <path>"
    exit 1
fi


# ================================================================
# Validate OOpenCAL directory
# ================================================================
if [[ ! -d "$OOPENCAL_DIR/OOpenCAL" ]]; then
    log_err "Directory does not contain OOpenCAL/:"
    echo "  $OOPENCAL_DIR/OOpenCAL"
    exit 1
fi

log_ok "OOpenCAL found at: $OOPENCAL_DIR"
echo


# ================================================================
# Detect OS
# ================================================================
OS="unknown"
if [[ -f /etc/os-release ]]; then
    source /etc/os-release
    OS="$ID"
fi

log_info "Detected OS: $OS"
log_info "Using Qt mode: ${QT_VERSION^^}"
echo


# ================================================================
# Debian/Ubuntu - package checks
# ================================================================
if [[ "$OS" == "ubuntu" || "$OS" == "debian" ]]; then

    log_step "Checking required system packages (APT mode)"

    if [[ "$QT_VERSION" == "qt6" ]]; then
        REQUIRED_PACKAGES=(
            build-essential cmake
            qt6-base-dev qt6-tools-dev qt6-tools-dev-tools
            libvtk9-dev libvtk9-qt-dev
            libgl1-mesa-dev libglu1-mesa-dev
            libeigen3-dev libfreetype-dev
            libpng-dev libjpeg-dev libtiff-dev zlib1g-dev
        )
        
        # WARNING: libvtk9-qt-dev on Ubuntu 24.04 is compiled with Qt5, not Qt6
        # This may cause linking issues. Consider using Qt5 mode if you encounter
        # VTK-related linker errors.
        log_warn "libvtk9-qt-dev on Ubuntu 24.04 is compiled with Qt5, not Qt6"
        log_warn "If you encounter VTK linking errors, try: ./build_viewer.sh --oopencal <path> --qt=5"
        
    else
        REQUIRED_PACKAGES=(
            build-essential cmake
            qtbase5-dev qttools5-dev qttools5-dev-tools
            libvtk9-dev libvtk9-qt-dev
            libgl1-mesa-dev libglu1-mesa-dev
            libeigen3-dev libfreetype-dev
            libpng-dev libjpeg-dev libtiff-dev zlib1g-dev
        )
    fi

    MISSING=()
    for pkg in "${REQUIRED_PACKAGES[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            MISSING+=("$pkg")
        fi
    done

    if [[ ${#MISSING[@]} -gt 0 ]]; then
        echo
        log_err "Missing required packages:"
        for p in "${MISSING[@]}"; do
            echo "  - $p"
        done
        echo
        echo -e "${YELLOW}Install them using:${RESET}"
        echo "  sudo apt update && sudo apt install -y ${MISSING[*]}"
        echo
        exit 1
    fi

    log_ok "All required APT packages installed."

else
    # Non-Debian systems
    echo -e "${YELLOW}=============================================================="
    echo -e " NON-DEBIAN LINUX DETECTED"
    echo -e " Please ensure the following are installed:"
    echo -e "    Qt${QT_VERSION: -1} development packages"
    echo -e "    VTK with Qt support"
    echo -e "    CMake, GCC/Clang"
    echo -e "==============================================================${RESET}"
fi


# ================================================================
# Build using CMake + Make
# ================================================================
BUILD_DIR="${VIEWER_ROOT}/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if command -v ninja >/dev/null 2>&1; then
    log_warn "Ninja detected but NOT used (forced Makefiles)."
fi

log_step "Configuring CMake..."
cmake "$VIEWER_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_QT6=$( [[ "$QT_VERSION" == "qt6" ]] && echo ON || echo OFF ) \
    -DOOPENCAL_DIR="$OOPENCAL_DIR" \
    -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF

log_step "Building (make)..."
make -j"$(nproc)"


# ================================================================
# Print result
# ================================================================
EXECUTABLE="$(find "$BUILD_DIR" -type f -executable -name 'OOpenCal-Viewer' | head -n 1)"

echo
echo -e "${GREEN}${BOLD}==========================================${RESET}"
echo -e "${GREEN}${BOLD} Build completed successfully!${RESET}"
echo -e "${GREEN}${BOLD} Build directory:${RESET} $BUILD_DIR"
echo -e "${GREEN}${BOLD} Executable:${RESET} $EXECUTABLE"
echo -e "${GREEN}${BOLD}==========================================${RESET}"
echo
