#!/usr/bin/env bash
# ==============================================================
# build_plugin.sh
#
# Helper script to build a plugin module for OOpenCAL / OOpenCal-Visualiser.
#
# Features:
#   - Automatically prepares build directory near the provided header
#   - Symlinks CMakeLists.txt and Plugin_FullTemplate.cpp
#   - Allows specifying an optional template plugin directory
#   - If not given, it tries to locate it automatically via OOPENCALVIEWER_DIR
#   - Passes through all CMake arguments
#   - Uses a precompiled header shared by all plugins of one Viewer (built on first use, ~25% faster builds),
#     see doc/PRECOMPILED_HEADER.md. Any problem with it only means a normal build.
#     Set OOPENCAL_NO_PRECOMPILED_HEADER=1 to switch it off.
#
# Usage:
#   ./build_plugin.sh path/to/MyCell.h [--template /path/to/plugin_template]
#                    [--includes /path/to/include] [CMAKE_ARGS...]
#   ./build_plugin.sh --prepare-precompiled-header [--template ...] [--includes ...] [CMAKE_ARGS...]
#       (only builds the shared precompiled header; the same -DOOPENCAL_DIR, -DOOPENCALVIEWER_DIR and --includes
#        as for the plugins have to be given, they decide which header is reused)
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
    PREPARE_ONLY=0
    HEADER_FILE=""

    if [[ "${1:-}" == "--prepare-precompiled-header" ]]; then
        PREPARE_ONLY=1
        shift
    elif [[ $# -lt 1 ]]; then
        die "Usage: $0 path/to/MyCell.h [--template path] [--includes path] [CMAKE_ARGS...]"
    else
        HEADER_FILE="$1"
        shift
    fi

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

    if [[ "$PREPARE_ONLY" -eq 0 ]]; then
        [[ -f "$HEADER_FILE" ]] || die "Header file '$HEADER_FILE' not found"

        HEADER_DIR="$(cd "$(dirname "$HEADER_FILE")" && pwd)"
        HEADER_BASE="$(basename "$HEADER_FILE")"
        HEADER_NAME="${HEADER_BASE%.*}"
        BUILD_DIR="${HEADER_DIR}/build"
    fi
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
# Small helpers for the precompiled header
# --------------------------------------------------------------
now_ms() {
    if [[ -n "${EPOCHREALTIME:-}" ]]; then
        local microseconds="${EPOCHREALTIME/[.,]/}"
        echo $(( microseconds / 1000 ))
    else
        echo $(( $(date +%s) * 1000 ))   # old bash (macOS): whole seconds only
    fi
}

format_duration_ms() {
    local ms="$1"
    printf '%d.%02d s' $(( ms / 1000 )) $(( (ms % 1000) / 10 ))
}

# Absolute path without symlinks (the same directory always gives the same text); unchanged if it does not exist
canonical_path() {
    local dir="$1"
    if [[ -n "$dir" && -d "$dir" ]]; then
        (cd "$dir" && pwd -P)
    else
        printf '%s' "$dir"
    fi
}

# Value of -D<name>=<value> among the CMake arguments (empty if there is none)
cmake_arg_value() {
    local name="$1" arg
    for arg in ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}; do
        if [[ "$arg" == -D${name}=* ]]; then
            printf '%s' "${arg#-D${name}=}"
            return 0
        fi
    done
    return 0
}

# --------------------------------------------------------------
# Optional: precompiled header shared by all plugins of one Viewer
# --------------------------------------------------------------
# Every plugin parses the same Viewer / OOpenCAL headers, which is a big part of the build time. They are built once
# (the same CMake project as the plugin, switched to OOPENCAL_PLUGIN_PCH_ONLY, so the compiler flags are identical
# by construction) in a directory of the Viewer build and reused by every plugin. The directory is created
# on first use; make rebuilds it when the headers change. Best effort: any problem => the plugin is built without it.
PCH_CMAKE_ARGS=("-DOOPENCAL_PRECOMPILED_HEADER_DIR=")
PCH_STATUS="without precompiled header"

prepare_precompiled_header() {
    PCH_CMAKE_ARGS=("-DOOPENCAL_PRECOMPILED_HEADER_DIR=")
    PCH_STATUS="without precompiled header"

    if [[ "${OOPENCAL_NO_PRECOMPILED_HEADER:-0}" == "1" ]]; then
        info "Precompiled header is switched off (OOPENCAL_NO_PRECOMPILED_HEADER=1)"
        return 0
    fi

    local viewer_dir oopencal_dir includes_dir
    viewer_dir="$(canonical_path "$(cmake_arg_value OOPENCALVIEWER_DIR)")"
    [[ -n "$viewer_dir" ]] || viewer_dir="$(canonical_path "$TEMPLATE_DIR/../..")"
    oopencal_dir="$(canonical_path "$(cmake_arg_value OOPENCAL_DIR)")"
    includes_dir="$(canonical_path "$EXTRA_INCLUDE_DIR")"

    # Arguments for the build of the header: everything except what is specific to one plugin.
    # The other arguments (compiler, build type...) change the flags, so they are a part of the key as well.
    local producer_args=() other_args="" arg
    for arg in ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}; do
        case "$arg" in
            -DPLUGIN_*|-DOOPENCAL_PRECOMPILED_HEADER_DIR=*) ;;
            -DOOPENCAL_DIR=*|-DOOPENCALVIEWER_DIR=*) producer_args+=("$arg") ;;
            *) producer_args+=("$arg"); other_args+="$arg " ;;
        esac
    done
    if [[ -n "$EXTRA_INCLUDE_DIR" ]]; then
        producer_args+=("-DEXTRA_INCLUDE_DIR=${EXTRA_INCLUDE_DIR}")
    fi

    local compiler="${CXX:-c++}" compiler_id key
    compiler_id="$("$compiler" --version 2>/dev/null | head -n 1 || true)"
    key="$(printf '%s' "${viewer_dir}|${oopencal_dir}|${includes_dir}|${compiler}|${compiler_id}|${other_args}" | cksum | cut -d' ' -f1)"

    local pch_build_dir="${viewer_dir}/build/plugin-precompiled-header/${key}"
    local log_file="${pch_build_dir}.log"
    if ! mkdir -p "$(dirname "$pch_build_dir")" 2>/dev/null; then
        info "Precompiled header is not available (cannot create a directory in $viewer_dir), building without it"
        return 0
    fi

    info "Preparing the precompiled header shared by all plugins: $pch_build_dir"
    local started ok=0 configured_now attempt
    started="$(now_ms)"
    : > "$log_file"
    for attempt in 1 2; do
        # Configured only once per key (the key already holds everything which decides about the flags). Configuring is
        # expensive (find_package(VTK)), whereas `cmake --build` is cheap when nothing changed and regenerates
        # the build system by itself when the CMake files of the template changed.
        configured_now=0
        if [[ ! -f "$pch_build_dir/CMakeCache.txt" ]]; then
            configured_now=1
            if ! cmake -S "$TEMPLATE_DIR" -B "$pch_build_dir" -DOOPENCAL_PLUGIN_PCH_ONLY=ON ${producer_args[@]+"${producer_args[@]}"} >> "$log_file" 2>&1; then
                rm -rf "$pch_build_dir"
                break
            fi
        fi
        if cmake --build "$pch_build_dir" -j"$(nproc)" >> "$log_file" 2>&1; then
            ok=1
            break
        fi
        # A leftover of an interrupted or outdated configuration is possible: start from scratch (once)
        rm -rf "$pch_build_dir"
        [[ "$configured_now" -eq 1 ]] && break
    done

    if [[ "$ok" -eq 1 ]]; then
        info "Precompiled header is ready ($(format_duration_ms $(( $(now_ms) - started ))))"
        PCH_CMAKE_ARGS=("-DOOPENCAL_PRECOMPILED_HEADER_DIR=${pch_build_dir}")
        PCH_STATUS="with precompiled header"
    else
        info "Precompiled header could not be prepared (see $log_file), building without it"
    fi
    return 0
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
# Configures and builds the plugin once. The output of make is kept in $MAKE_LOG, so it can be checked afterwards.
configure_and_build() {
    local cmake_cmd=("cmake" "." "${CMAKE_ARGS[@]}")
    if [[ -n "$EXTRA_INCLUDE_DIR" ]]; then
        cmake_cmd+=("-DEXTRA_INCLUDE_DIR=${EXTRA_INCLUDE_DIR}")
    fi
    # Always given (empty when there is no precompiled header), so a value cached by an earlier build cannot linger
    cmake_cmd+=("${PCH_CMAKE_ARGS[@]}")

    MAKE_LOG="${BUILD_DIR}/make-output.log"
    info "Running CMake configuration..."
    (
        cd "$BUILD_DIR" || exit 1
        "${cmake_cmd[@]}" || exit 1
        info "Building plugin..."
        make -j"$(nproc)" 2>&1 | tee "$MAKE_LOG"
    )
}

run_cmake_build() {
    local started
    started="$(now_ms)"

    if ! configure_and_build; then
        if [[ "$PCH_STATUS" != "with precompiled header" ]]; then
            return 1
        fi
        # clang reports a precompiled header it cannot use as an error. It is only an optimization: build without it.
        info "The build with the precompiled header failed, trying again without it..."
        PCH_CMAKE_ARGS=("-DOOPENCAL_PRECOMPILED_HEADER_DIR=")
        PCH_STATUS="without precompiled header (the compiler could not use it)"
        configure_and_build
    elif [[ "$PCH_STATUS" == "with precompiled header" ]] && grep -q "\[-Winvalid-pch\]" "$MAKE_LOG"; then
        # g++ only warns and silently reads the headers as usual: say so instead of claiming a speedup
        local reason
        reason="$(grep -m1 "\[-Winvalid-pch\]" "$MAKE_LOG" | sed -e 's/^.*gch: //' -e 's/ \[-Winvalid-pch\]//')"
        PCH_STATUS="precompiled header NOT used by the compiler: ${reason}"
    fi

    info "Plugin built in $(format_duration_ms $(( $(now_ms) - started ))) (${PCH_STATUS})"
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

    prepare_precompiled_header

    if [[ "$PREPARE_ONLY" -eq 1 ]]; then
        if [[ "$PCH_STATUS" != "with precompiled header" && "${OOPENCAL_NO_PRECOMPILED_HEADER:-0}" != "1" ]]; then
            die "The precompiled header could not be prepared"
        fi
        return 0
    fi

    prepare_build_dir
    link_required_files
    run_cmake_build
    move_output_library

    info "✅ Plugin build completed successfully."
}

main "$@"
