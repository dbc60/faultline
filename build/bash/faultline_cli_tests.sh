#!/usr/bin/env bash
# faultline_cli_tests.sh - Build faultline_cli_tests.dll.
# Mirrors build/cmd/faultline_cli_tests.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Faultline CLI"

if [[ $build -eq 1 ]]; then
    _compiler="$CLANG"
    _compiler_flags="$COMMON_COMPILER_FLAGS"
    _linker_flags="$COMMON_LINKER_FLAGS"

    # faultline.sh builds the shared objects; build them here only when missing.
    if [[ ! -f "$DIR_OUT_OBJ/sqlite3.o" ]]; then
        [[ $verbose -eq 1 ]] && echo "Compile sqlite3"
        "$CLANG" -target x86_64-w64-mingw32 -std=c17 -w \
            -c "$DIR_THIRD_PARTY/sqlite/sqlite3.c" \
            -o "$DIR_OUT_OBJ/sqlite3.o"
    fi
    if [[ ! -f "$DIR_OUT_OBJ/cwalk.o" ]]; then
        [[ $verbose -eq 1 ]] && echo "Compile cwalk"
        "$CLANG" -target x86_64-w64-mingw32 -std=c17 -w \
            -I "$DIR_THIRD_PARTY/cwalk/include" \
            -c "$DIR_THIRD_PARTY/cwalk/src/cwalk.c" \
            -o "$DIR_OUT_OBJ/cwalk.o"
    fi

    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME test suite"

    "$_compiler" $_compiler_flags -DDLL_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" -I "$DIR_THIRD_PARTY/cwalk/include" \
        -I "$DIR_REPO/src" -I "$DIR_REPO/app/faultline" \
        -c "$DIR_REPO/src/faultline_cli_tests.c" \
        -o "$DIR_OUT_OBJ/faultline_cli_tests.o" \
        -MJ "$DIR_OUT_OBJ/faultline_cli_tests.json"

    "$_compiler" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/faultline_cli_tests.o" \
        "$DIR_OUT_OBJ/sqlite3.o" \
        "$DIR_OUT_OBJ/cwalk.o" \
        $_linker_flags \
        -o "$DIR_OUT_BIN/faultline_cli_tests.dll"
fi

if [[ $test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./faultline.exe run faultline_cli_tests.dll
    ./faultline.exe result list --limit 1
    popd > /dev/null
fi
