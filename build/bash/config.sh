#!/usr/bin/env bash
# config.sh - Detect compiler/toolchain paths and set compiler/linker flag variables.
# Source this script after options.sh; do not execute it directly.
# Expects $release and $trace to be set by options.sh.

# --- Clang binary detection ---
# Primary location; use POSIX path for bash file test in MSYS2/MINGW64
_CLANG_POSIX="/c/Users/dougc/llvm21/bin/clang.exe"
_CLANG_WIN="C:/Users/dougc/llvm21/bin/clang.exe"

if [[ -f "$_CLANG_POSIX" ]]; then
    CLANG="$_CLANG_WIN"
elif command -v clang &>/dev/null; then
    CLANG="clang"
else
    echo "config.sh: ERROR: clang not found at '$_CLANG_WIN' and not on PATH" >&2
    return 1
fi
unset _CLANG_POSIX _CLANG_WIN

# --- MinGW sysroot detection ---
# Used to find standard headers (windows.h, stdio.h, etc.)
MINGW_SYSROOT=""
if command -v x86_64-w64-mingw32-gcc &>/dev/null; then
    _MINGW_GCC="$(command -v x86_64-w64-mingw32-gcc)"
    _MINGW_DIR="$(dirname "$(dirname "$_MINGW_GCC")")"
    # Convert to mixed Windows path so clang.exe can use it
    if command -v cygpath &>/dev/null; then
        MINGW_SYSROOT="$(cygpath -m "$_MINGW_DIR")"
    else
        MINGW_SYSROOT="$_MINGW_DIR"
    fi
    unset _MINGW_GCC _MINGW_DIR
elif [[ -d "/mingw64" ]]; then
    if command -v cygpath &>/dev/null; then
        MINGW_SYSROOT="$(cygpath -m /mingw64)"
    else
        MINGW_SYSROOT="/mingw64"
    fi
fi

# --- MinGW GCC private include directory ---
# MinGW's malloc.h does #include <mm_malloc.h>, which lives in GCC's private
# include directory (lib/gcc/x86_64-w64-mingw32/<ver>/include/), NOT in the
# sysroot's main include/ dir. Clang's --sysroot never adds this directory, so
# we detect it by globbing and add it explicitly via -isystem.
MINGW_GCC_INC=""
if [[ -n "$MINGW_SYSROOT" ]]; then
    _sysroot_posix="$(cygpath -u "$MINGW_SYSROOT" 2>/dev/null || echo "$MINGW_SYSROOT")"
    for _gcc_dir in "$_sysroot_posix/lib/gcc/x86_64-w64-mingw32"/*/include; do
        [[ -d "$_gcc_dir" ]] || continue
        MINGW_GCC_INC="$(cygpath -m "$_gcc_dir" 2>/dev/null || echo "$_gcc_dir")"
        break
    done
    unset _sysroot_posix _gcc_dir
fi

# --- Common compiler flags (both debug and release) ---
_COMMON_BASE="-target x86_64-w64-mingw32
    -std=c17
    -Wall -Wextra -Werror
    -fno-stack-protector
    -D_UNICODE -DUNICODE -D_WIN32 -DWIN32 -D__STDC_WANT_LIB_EXT1__=1"

if [[ -n "$MINGW_SYSROOT" ]]; then
    _COMMON_BASE="$_COMMON_BASE --sysroot=$MINGW_SYSROOT"
fi

if [[ $release -eq 1 ]]; then
    COMMON_COMPILER_FLAGS="$_COMMON_BASE -O2 -g -flto"
else
    COMMON_COMPILER_FLAGS="$_COMMON_BASE -O0 -g -DDEBUG"
fi
unset _COMMON_BASE

# --- Common linker flags ---
#COMMON_LINKER_FLAGS="-Wl,--stack,1048576"
# -fuse-ld=lld: use LLVM's LLD instead of MinGW's ld. Required for release
# builds because -flto produces LLVM bitcode objects that GNU ld cannot read.
COMMON_LINKER_FLAGS="-fuse-ld=lld -Wl,--stack,1048576"

if [[ $trace -eq 1 ]]; then
    echo "CONFIG.SH: configuration"
    echo "  CLANG=$CLANG"
    echo "  MINGW_SYSROOT=$MINGW_SYSROOT"
    echo "  MINGW_GCC_INC=$MINGW_GCC_INC"
    echo "  COMMON_COMPILER_FLAGS=$COMMON_COMPILER_FLAGS"
    echo "  COMMON_LINKER_FLAGS=$COMMON_LINKER_FLAGS"
fi
