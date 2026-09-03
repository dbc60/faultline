#!/usr/bin/env bash
# config.sh - Detect compiler/toolchain paths and set compiler/linker flag variables.
# Source this script after options.sh; do not execute it directly.
# Expects $release and $trace to be set by options.sh.

# --- Clang binary detection ---
# Primary location; use POSIX path for bash file test in MSYS2/MINGW64
_CLANG_POSIX="/c/Users/dougc/llvm22/bin/clang.exe"
_CLANG_WIN="C:/Users/dougc/llvm22/bin/clang.exe"

if [[ -f "$_CLANG_POSIX" ]]; then
    CLANG="$_CLANG_WIN"
elif command -v clang &>/dev/null; then
    CLANG="clang"
else
    echo "config.sh: ERROR: clang not found at '$_CLANG_WIN' and not on PATH" >&2
    return 1
fi
unset _CLANG_POSIX _CLANG_WIN

# --- Clang++ binary detection (cxx build option) ---
_CLANGXX_POSIX="/c/Users/dougc/llvm22/bin/clang++.exe"
_CLANGXX_WIN="C:/Users/dougc/llvm22/bin/clang++.exe"

if [[ -f "$_CLANGXX_POSIX" ]]; then
    CLANGXX="$_CLANGXX_WIN"
elif command -v clang++ &>/dev/null; then
    CLANGXX="clang++"
else
    echo "config.sh: ERROR: clang++ not found at '$_CLANGXX_WIN' and not on PATH" >&2
    return 1
fi
unset _CLANGXX_POSIX _CLANGXX_WIN

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
# -Wformat-nonliteral is not in -Wall or -Wextra. -Wformat-security, which is,
# only fires on a runtime format with no arguments; a runtime format that does
# take arguments -- a caught message spliced into one, say -- goes unremarked
# without this. Worth the flag: the LOG_* family and FL_THROW_DETAILS both take
# a format from their caller.
_COMMON_BASE="-target x86_64-w64-mingw32
    -Wall -Wextra -Werror
    -Wformat-nonliteral
    -fno-stack-protector
    -D_UNICODE -DUNICODE -D_WIN32 -DWIN32 -D__STDC_WANT_LIB_EXT1__=1"

if [[ -n "$MINGW_SYSROOT" ]]; then
    _COMMON_BASE="$_COMMON_BASE --sysroot=$MINGW_SYSROOT"
fi

# A second, C++ dialect flag set, mechanically derived from the C one: swap
# -std=c17 for -std=c++20 (C++20, not C++17: the tree uses designated
# initializers throughout) and define FL_EXC_BACKEND_CXX. Mirrors config.cmd's
# CommonCompilerFlagsCXX split; unlike that one, there is no /EHc analogue to
# strip here. clang++ (CLANGXX) drives this set, same as cl does with /TP.
# -x c++ is required alongside clang++: without it, clang++ still detects the
# .c extension and, under -Werror, turns its own "treating 'c' input as
# 'c++' ... deprecated" notice into a hard error.
#
# -Wno-deprecated-anon-enum-enum-conversion: FNV64.c does arithmetic between
# two different enum types, which C++ deprecates. cl's analogous /wd5054
# suppresses the same finding on the MSVC path; FNV64.c is third-party, so
# the warning is suppressed rather than the source edited.
#
# -Wno-missing-designated-field-initializers: FL_TYPE_TEST_SETUP_CLEANUP
# (fl_test.h) designates only the leading .tc field of each test-case struct,
# relying on the remaining fields zero-initializing -- valid and intentional
# in both dialects, not a bug the C++ dialect newly exposes.
#
# -Wno-missing-field-initializers: the sibling diagnostic for positional
# aggregate init. C exempts the "= {0}" zero-the-whole-struct idiom from
# this warning; C++ does not, so every "= {0}" across the tree (there is no
# other spelling this codebase uses to zero-initialize a local struct) is
# a fresh error under -Werror. Same non-bug as above, just the positional
# rather than the designated form.
_CXX_ONLY="-x c++ -std=c++20 -DFL_EXC_BACKEND_CXX \
    -Wno-deprecated-anon-enum-enum-conversion \
    -Wno-missing-designated-field-initializers -Wno-missing-field-initializers"

if [[ $release -eq 1 ]]; then
    COMMON_COMPILER_FLAGS="$_COMMON_BASE -std=c17 -O2 -g -flto"
    COMMON_COMPILER_FLAGS_CXX="$_COMMON_BASE $_CXX_ONLY -O2 -g -flto"
else
    COMMON_COMPILER_FLAGS="$_COMMON_BASE -std=c17 -O0 -g -DDEBUG"
    COMMON_COMPILER_FLAGS_CXX="$_COMMON_BASE $_CXX_ONLY -O0 -g -DDEBUG"
fi
unset _COMMON_BASE _CXX_ONLY

# --- Common linker flags ---
#COMMON_LINKER_FLAGS="-Wl,--stack,1048576"
# -fuse-ld=lld: use LLVM's LLD instead of MinGW's ld. Required for every build,
# not just release. Release needs it because -flto produces LLVM bitcode objects
# GNU ld cannot read -- a loud failure. Every build needs it because clang emits
# native Windows TLS for this target, and GNU ld lays a spurious base relocation
# over the section-relative displacement: ASLR then rebases a value that must not
# be rebased, and the first read of any FL_THREAD_LOCAL object faults. That one
# is silent until it segfaults, so do not narrow this to release builds.
COMMON_LINKER_FLAGS="-fuse-ld=lld -Wl,--stack,1048576"

# The C++ link line adds -static-libstdc++ -static-libgcc: three independent
# copies of the runtime linked into faultline.exe, but_driver.exe, and every
# test DLL, rather than one libstdc++-6.dll all of them would otherwise need
# on the PATH beside them. This sysroot's libstdc++ is the posix-threading
# build, so its exception-handling TLS support (eh_alloc.o, emutls.o) pulls
# in pthread_* symbols regardless -- -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
# resolves those from the static archive; plain -lwinpthread would resolve
# them from the import library instead, adding a libwinpthread-1.dll
# dependency the static-libstdc++/-libgcc pair above was meant to avoid.
COMMON_LINKER_FLAGS_CXX="$COMMON_LINKER_FLAGS -static-libstdc++ -static-libgcc \
    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic"

if [[ $trace -eq 1 ]]; then
    echo "CONFIG.SH: configuration"
    echo "  CLANG=$CLANG"
    echo "  CLANGXX=$CLANGXX"
    echo "  MINGW_SYSROOT=$MINGW_SYSROOT"
    echo "  MINGW_GCC_INC=$MINGW_GCC_INC"
    echo "  COMMON_COMPILER_FLAGS=$COMMON_COMPILER_FLAGS"
    echo "  COMMON_COMPILER_FLAGS_CXX=$COMMON_COMPILER_FLAGS_CXX"
    echo "  COMMON_LINKER_FLAGS=$COMMON_LINKER_FLAGS"
    echo "  COMMON_LINKER_FLAGS_CXX=$COMMON_LINKER_FLAGS_CXX"
fi
