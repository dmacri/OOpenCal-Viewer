#!/usr/bin/env bash
# Runs every unit-test binary in the build tree under valgrind and writes one combined report.
#
# Usage: run-valgrind-tests.sh <build-dir> <valgrind-binary> <report-file>
#
# Exit status is non-zero if any test fails or valgrind reports an error.
# Leaks from the Qt/VTK runtime can be silenced by adding entries to valgrind.supp
# in the repository root; the file is picked up automatically when present.

set -uo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 <build-dir> <valgrind-binary> <report-file>" >&2
    exit 2
fi

build_dir=$1
valgrind_bin=$2
report=$3

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)

suppressions_args=()
if [[ -f "$repo_root/valgrind.supp" ]]; then
    suppressions_args=(--suppressions="$repo_root/valgrind.supp")
fi

# Tests run headless (CI has no display).
export QT_QPA_PLATFORM=offscreen

mapfile -t tests < <(find "$build_dir" -type f -executable -name '*Tests' | sort)

: > "$report"
if (( ${#tests[@]} == 0 )); then
    echo "No test binaries (*Tests) found under $build_dir" | tee -a "$report"
    exit 1
fi

status=0
for test_bin in "${tests[@]}"; do
    echo "=== valgrind: $test_bin" | tee -a "$report"
    "$valgrind_bin" \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --num-callers=20 \
        --error-exitcode=1 \
        "${suppressions_args[@]}" \
        "$test_bin" 2>&1 | tee -a "$report"
    rc=${PIPESTATUS[0]}
    if (( rc != 0 )); then
        echo "=== FAILED (exit $rc): $test_bin" | tee -a "$report"
        status=1
    else
        echo "=== OK: $test_bin" | tee -a "$report"
    fi
done

exit $status
