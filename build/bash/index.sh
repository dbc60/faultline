#!/usr/bin/env bash
# index.sh - Build index_generic_tests.dll and index_windows_tests.dll.
# Mirrors build/cmd/index.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Index"

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/index.ctm"
fi

if [[ $build -eq 1 ]]; then
    # --- index_generic_tests.dll ---
    [[ $verbose -eq 1 ]] && echo "Build Generic $PROJECT_NAME test suite"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
        -I "$DIR_INCLUDE" \
        -c "$DIR_REPO/src/index_generic_tests.c" \
        -o "$DIR_OUT_OBJ/index_generic_tests.o" \
        -MJ "$DIR_OUT_OBJ/index_generic_tests.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/index_generic_tests.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/index_generic_tests.dll"

    # --- index_windows_tests.dll ---
    [[ $verbose -eq 1 ]] && echo "Build Windows $PROJECT_NAME test suite"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
        -I "$DIR_INCLUDE" \
        -c "$DIR_REPO/src/index_windows_tests.c" \
        -o "$DIR_OUT_OBJ/index_windows_tests.o" \
        -MJ "$DIR_OUT_OBJ/index_windows_tests.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/index_windows_tests.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/index_windows_tests.dll"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/index.ctm" $?
fi

if [[ $test -eq 1 ]]; then
    if [[ ! -f "$DIR_OUT_BIN/but_driver.exe" ]]; then
        rel_arg=$([[ $release -eq 1 ]] && echo "release" || echo "")
        bash "$SCRIPT_DIR/but_driver.sh" build $rel_arg
    fi
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME unit tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./but_driver.exe index_generic_tests.dll
    ./but_driver.exe index_windows_tests.dll
    popd > /dev/null
fi
