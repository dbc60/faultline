#!/usr/bin/env bash
# but_driver.sh - Build but_tests.dll, but_test_data.dll, and but_driver.exe.
# Mirrors build/app/but_driver.cmd.
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
    # The first-party sources take whichever dialect cxx picked.
    _compiler="$CLANG"
    _compiler_flags="$COMMON_COMPILER_FLAGS"
    _linker_flags="$COMMON_LINKER_FLAGS"
    if [[ $cxx -eq 1 ]]; then
        _compiler="$CLANGXX"
        _compiler_flags="$COMMON_COMPILER_FLAGS_CXX"
        _linker_flags="$COMMON_LINKER_FLAGS_CXX"
    fi

    # --- but_tests.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME test suite"

    "$_compiler" $_compiler_flags -DDLL_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/but_tests.c" \
        -o "$DIR_OUT_OBJ/but_tests.o" \
        -MJ "$DIR_OUT_OBJ/but_tests.json"

    "$_compiler" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/but_tests.o" \
        $_linker_flags \
        -o "$DIR_OUT_BIN/but_tests.dll"

    # --- but_test_data.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME driver test-data DLL"

    "$_compiler" $_compiler_flags -DDLL_BUILD -DFL_PLATFORM_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/but_test_data.c" \
        -o "$DIR_OUT_OBJ/but_test_data.o" \
        -MJ "$DIR_OUT_OBJ/but_test_data.json"

    "$_compiler" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/but_test_data.o" \
        $_linker_flags \
        -o "$DIR_OUT_BIN/but_test_data.dll"

    # --- but_driver.exe ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME Driver"

    "$_compiler" $_compiler_flags -DFL_PLATFORM_BUILD -DFL_EMBEDDED \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" -I "$DIR_REPO/src" \
        -c "$DIR_REPO/app/but/win32_main.c" \
        -o "$DIR_OUT_OBJ/win32_main.o" \
        -MJ "$DIR_OUT_OBJ/win32_main.json"

    "$_compiler" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/win32_main.o" \
        $_linker_flags \
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
