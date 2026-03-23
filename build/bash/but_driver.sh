#!/usr/bin/env bash
# but_driver.sh - Build but_tests.dll, but_test_data.dll, and but_driver.exe.
# Mirrors build/cmd/but_driver.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="BUT Test Driver"

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/but_test_driver.ctm"
fi

if [[ $build -eq 1 ]]; then
    # --- but_tests.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME test suite"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/but_tests.c" \
        -o "$DIR_OUT_OBJ/but_tests.o" \
        -MJ "$DIR_OUT_OBJ/but_tests.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/but_tests.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/but_tests.dll"

    # --- but_test_data.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME driver test-data DLL"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD -DFL_BUILD_DRIVER \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/but_test_data.c" \
        -o "$DIR_OUT_OBJ/but_test_data.o" \
        -MJ "$DIR_OUT_OBJ/but_test_data.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/but_test_data.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/but_test_data.dll"

    # --- but_driver.exe ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME Driver"

    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_BUILD_DRIVER -DFL_EMBEDDED \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/cmd/but/win32_main.c" \
        -o "$DIR_OUT_OBJ/win32_main.o" \
        -MJ "$DIR_OUT_OBJ/win32_main.json"

    "$CLANG" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/win32_main.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/but_driver.exe"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/but_test_driver.ctm" $?
fi

if [[ $test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./but_driver.exe but_tests.dll
    popd > /dev/null
fi
