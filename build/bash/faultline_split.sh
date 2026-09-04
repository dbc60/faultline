#!/usr/bin/env bash
# faultline_split.sh - Build win32_faultline.exe, the split-architecture host.
# Mirrors build/cmd/faultline_split.cmd: compiles the Win32 platform layer
# (win32_faultline_unity.c) and links it with faultline_core.o plus the shared
# sqlite3.o / cwalk.o.
#
# Pipeline:
#   1. sqlite3.o / cwalk.o    -> built here only when missing (faultline.sh owns them)
#   2. faultline_core.sh      -> faultline_core.o (the OS-free app layer)
#   3. faultline.sh           -> faultline_tests.dll (only when testing and missing)
#   4. this script            -> win32_faultline.exe (the platform layer)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Faultline Split"

# Sub-builds must not repeat the clean this script's setup already performed.
# Forward the configuration options but strip the clean/test verbs (test maps
# to build), the same filtering all.sh uses.
_forward_args=()
for _a in "$@"; do
    case "$_a" in
        test) _forward_args+=(build) ;;
        clean|cleanall) ;;
        *) _forward_args+=("$_a") ;;
    esac
done
unset _a

if [[ $build -eq 1 ]]; then
    # Shared fixtures must exist before the link step; faultline.sh normally
    # builds them, so only rebuild here when missing (e.g. after a clean).
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

    # Build the OS-free application layer first.
    bash "$SCRIPT_DIR/faultline_core.sh" "${_forward_args[@]}"
fi

# The test step below runs faultline_tests.dll; after a clean it is gone.
# faultline.sh rebuilds it (and, alongside, the monolithic faultline.exe).
if [[ $test -eq 1 && ! -f "$DIR_OUT_BIN/faultline_tests.dll" ]]; then
    bash "$SCRIPT_DIR/faultline.sh" "${_forward_args[@]}"
fi

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/faultline_split.ctm"
fi

if [[ $build -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME platform layer (unity): win32_faultline.exe"

    # Always C. cxx selects the dialect of a test suite, and this is not one.
    _compiler="$CLANG"
    _compiler_flags="$COMMON_COMPILER_FLAGS"
    _linker_flags="$COMMON_LINKER_FLAGS"

    "$_compiler" $_compiler_flags -DFL_PLATFORM_BUILD -DFL_EMBEDDED \
        -I "$DIR_INCLUDE" -I "$DIR_REPO/src" -I "$DIR_THIRD_PARTY" \
        -I "$DIR_THIRD_PARTY/cwalk/include" \
        -c "$DIR_REPO/app/faultline/win32_faultline_unity.c" \
        -o "$DIR_OUT_OBJ/win32_faultline.o" \
        -MJ "$DIR_OUT_OBJ/win32_faultline.json"

    "$_compiler" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/win32_faultline.o" \
        "$DIR_OUT_OBJ/faultline_core.o" \
        "$DIR_OUT_OBJ/sqlite3.o" \
        "$DIR_OUT_OBJ/cwalk.o" \
        $_linker_flags \
        -o "$DIR_OUT_BIN/win32_faultline.exe"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/faultline_split.ctm" $?
fi

if [[ $test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME unit tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./win32_faultline.exe run faultline_tests.dll
    ./win32_faultline.exe show results --limit 1
    popd > /dev/null
fi

unset _forward_args
