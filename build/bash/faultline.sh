#!/usr/bin/env bash
# faultline.sh - Build faultline_tests.dll, faultline_test_data.dll, and faultline.exe.
# Mirrors build/cmd/faultline.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="FaultLine"

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/faultline.ctm"
fi

if [[ $build -eq 1 ]]; then
    # --- sqlite3.o (shared by test DLL and exe; suppress all warnings) ---
    [[ $verbose -eq 1 ]] && echo "Compile sqlite3"
    "$CLANG" -target x86_64-w64-mingw32 -std=c17 -w \
        -c "$DIR_THIRD_PARTY/sqlite/sqlite3.c" \
        -o "$DIR_OUT_OBJ/sqlite3.o"

    # --- cwalk.o (shared by test DLL and exe; suppress all warnings) ---
    [[ $verbose -eq 1 ]] && echo "Compile cwalk"
    "$CLANG" -target x86_64-w64-mingw32 -std=c17 -w \
        -I "$DIR_THIRD_PARTY/cwalk/include" \
        -c "$DIR_THIRD_PARTY/cwalk/src/cwalk.c" \
        -o "$DIR_OUT_OBJ/cwalk.o"

    # --- faultline_tests.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME test suite"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/faultline_tests.c" \
        -o "$DIR_OUT_OBJ/faultline_tests.o" \
        -MJ "$DIR_OUT_OBJ/faultline_tests.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/faultline_tests.o" \
        "$DIR_OUT_OBJ/sqlite3.o" \
        "$DIR_OUT_OBJ/cwalk.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/faultline_tests.dll"

    # --- faultline_test_data.dll ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME driver test-data DLL"

    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD -DFL_BUILD_DRIVER \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/src/faultline_test_data.c" \
        -o "$DIR_OUT_OBJ/faultline_test_data.o" \
        -MJ "$DIR_OUT_OBJ/faultline_test_data.json"

    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/faultline_test_data.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/faultline_test_data.dll"

    # --- faultline.exe ---
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME driver"

    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_BUILD_DRIVER -DFL_EMBEDDED \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -c "$DIR_REPO/cmd/faultline/main_windows.c" \
        -o "$DIR_OUT_OBJ/faultline_main.o" \
        -MJ "$DIR_OUT_OBJ/faultline_main.json"

    "$CLANG" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/faultline_main.o" \
        "$DIR_OUT_OBJ/sqlite3.o" \
        "$DIR_OUT_OBJ/cwalk.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/faultline.exe"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/faultline.ctm" $?
fi

if [[ $test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./faultline.exe run faultline_tests.dll
    popd > /dev/null
fi
