#!/usr/bin/env bash
# faultline_core.sh - Build faultline_core.o, the OS-free application layer.
# Mirrors build/cmd/faultline_core.cmd. The cmd build archives faultline_core.lib;
# the clang build links objects directly, so the unit here is the object file.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Faultline Core"

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/faultline_core.ctm"
fi

if [[ $build -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME (unity, OS-free): faultline_core.o"

    # No -DFL_PLATFORM_BUILD: the core reaches OS capabilities only through the
    # services injected via the FLPlatformAPI. -DFL_EMBEDDED: the fla_ setters
    # are plain functions built into the binary, not DLL exports.
    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_EMBEDDED \
        -I "$DIR_INCLUDE" -I "$DIR_REPO/src" -I "$DIR_THIRD_PARTY" \
        -I "$DIR_THIRD_PARTY/cwalk/include" \
        -c "$DIR_REPO/app/faultline/faultline_core_unity.c" \
        -o "$DIR_OUT_OBJ/faultline_core.o" \
        -MJ "$DIR_OUT_OBJ/faultline_core.json"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/faultline_core.ctm" $?
fi
