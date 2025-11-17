#!/usr/bin/env bash
# ==============================================================
# build_plugin.sh
#
# Helper script to build a plugin module for OOpenCAL / Qt-VTK Viewer.
#
# Features:
#   - Automatically prepares build directory near the provided header
#   - Symlinks CMakeLists.txt and Plugin_FullTemplate.cpp
#   - Allows specifying an optional template plugin directory
#   - If not given, it tries to locate it automatically via OOPENCALVIEWER_DIR
#   - Passes through all CMake arguments
#
# Usage:
#   ./build_plugin.sh path/to/MyCell.h [--template /path/to/plugin_template]
#                    [--includes /path/to/include] [CMAKE_ARGS...]
#
# Example:
#   ./build_plugin.sh /home/user/OOpenCAL/models/Ball/Output/BallCell.h \
#       --template /home/user/OOpenCal-Viewer/examples/custom_model_plugin \
#       -DPLUGIN_MODEL_NAME='"Ball2"' \
#       -DPLUGIN_CELL_CLASS=BallCell \
#       -DOOPENCALVIEWER_DIR=/home/user/OOpenCal-Viewer \
#       -DOOPENCAL_DIR=/home/user/OOpenCAL \
#       --includes /home/user/OOpenCAL/base
# ==============================================================

set -euo pipefail

# --------------------------------------------------------------
# Utility helpers
# --------------------------------------------------------------
die()  { echo "❌ Error: $*" >&2; exit 1; }
info() { echo "🔹 $*"; }

# --------------------------------------------------------------
# Parse command-line arguments
# --------------------------------------------------------------
parse_args() {
    if [[ $# -lt 1 ]]; then
        die "Usage: $0 path/to/MyCell.h [--template path] [--includes path] [CMAKE_ARGS...]"
    fi

    HEADER_FILE="$1"
    shift

    TEMPLATE_DIR=""
    EXTRA_INCLUDE_DIR=""
    CMAKE_ARGS=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --template)
                shift
                [[ $# -gt 0 ]] || die "--template requires a path argument"
                TEMPLATE_DIR="$1"
                ;;
            --includes)
                shift
                [[ $# -gt 0 ]] || die "--includes requires a path argument"
                EXTRA_INCLUDE_DIR="$1"
                ;;
            *)
                CMAKE_ARGS+=("$1")
                ;;
        esac
        shift
    done

    [[ -f "$HEADER_FILE" ]] || die "Header file '$HEADER_FILE' not found"

    HEADER_DIR="$(cd "$(dirname "$HEADER_FILE")" && pwd)"
    HEADER_BASE="$(basename "$HEADER_FILE")"
    HEADER_NAME="${HEADER_BASE%.*}"
    BUILD_DIR="${HEADER_DIR}/build"
}

# --------------------------------------------------------------
# Resolve template plugin directory
# --------------------------------------------------------------
resolve_template_dir() {
    local DEFAULT_REL_PATH="examples/custom_model_plugin"
    local TEMPLATE_FILE="Plugin_FullTemplate.cpp"

    if [[ -z "$TEMPLATE_DIR" ]]; then
        # Try to infer from OOPENCALVIEWER_DIR CMake arg if provided
        local oopencalviewer_dir=""
        for arg in "${CMAKE_ARGS[@]}"; do
            if [[ "$arg" =~ ^-DOOPENCALVIEWER_DIR= ]]; then
                oopencalviewer_dir="${arg#-DOOPENCALVIEWER_DIR=}"
                oopencalviewer_dir="${oopencalviewer_dir%/}"
                break
            fi
        done

        if [[ -n "$oopencalviewer_dir" && -d "$oopencalviewer_dir/$DEFAULT_REL_PATH" ]]; then
            TEMPLATE_DIR="$oopencalviewer_dir/$DEFAULT_REL_PATH"
        fi
    fi

    # Validate template directory
    if [[ -z "$TEMPLATE_DIR" ]]; then
        die "Could not determine template plugin directory. Please specify with --template."
    fi

    [[ -d "$TEMPLATE_DIR" ]] || die "Template directory '$TEMPLATE_DIR' not found."
    [[ -f "$TEMPLATE_DIR/$TEMPLATE_FILE" ]] || die "Missing required file '$TEMPLATE_FILE' in '$TEMPLATE_DIR'."

    info "Using plugin template from: $TEMPLATE_DIR"
}

# --------------------------------------------------------------
# Prepare build directory
# --------------------------------------------------------------
prepare_build_dir() {
    info "Preparing build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
}

# --------------------------------------------------------------
# Link required files into the build directory
# --------------------------------------------------------------
link_required_files() {
    local REQUIRED_FILES=("CMakeLists.txt" "Plugin_FullTemplate.cpp")

    for FILE in "${REQUIRED_FILES[@]}"; do
        local SRC="$TEMPLATE_DIR/$FILE"
        if [[ -f "$SRC" ]]; then
            info "Linking $SRC → $BUILD_DIR"
            ln -sf "$(realpath "$SRC")" "$BUILD_DIR/$FILE"
        else
            die "Required file '$FILE' not found in template directory '$TEMPLATE_DIR'."
        fi
    done

    info "Linking header: $HEADER_FILE → $BUILD_DIR"
    ln -sf "$(realpath "$HEADER_FILE")" "$BUILD_DIR/$HEADER_BASE"
}

# --------------------------------------------------------------
# Run CMake and build
# --------------------------------------------------------------
run_cmake_build() {
    local cmake_cmd=("cmake" "." "${CMAKE_ARGS[@]}")
    if [[ -n "$EXTRA_INCLUDE_DIR" ]]; then
        cmake_cmd+=("-DEXTRA_INCLUDE_DIR=${EXTRA_INCLUDE_DIR}")
    fi

    info "Running CMake configuration..."
    (
        cd "$BUILD_DIR"
        "${cmake_cmd[@]}"
        info "Building plugin..."
        make -j"$(nproc)"
    )
}

# --------------------------------------------------------------
# Copy built .so library back next to the header file
# --------------------------------------------------------------
move_output_library() {
    local SO_FILE
    SO_FILE=$(find "$BUILD_DIR" -maxdepth 1 -type f -name "*.so" | head -n 1 || true)
    [[ -n "$SO_FILE" ]] || die "No .so file found in build directory."

    info "Copying result: $(basename "$SO_FILE") → $HEADER_DIR"
    cp -f "$SO_FILE" "$HEADER_DIR/"
}

# --------------------------------------------------------------
# Main build sequence
# --------------------------------------------------------------
main() {
    parse_args "$@"
    resolve_template_dir
    prepare_build_dir
    link_required_files
    run_cmake_build
    move_output_library

    info "✅ Plugin build completed successfully."
}

main "$@"
