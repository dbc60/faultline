#!/usr/bin/env bash
# config.sh - Detect compiler/toolchain paths and set compiler/linker flag variables
# and the FaultLine source lists.
# Source this script after options.sh; do not execute it directly.
# Expects $release, $cxx and $trace from options.sh, the directories from setup.sh,
# and DIR_FL_SRC, PROJECT_INCLUDES and PROJECT_COMPILER_FLAGS from project.sh.

# --- Clang binary detection ---
# CLANG in the environment pins an install. Otherwise take the primary location, then
# PATH; use the POSIX path for the bash file test in MSYS2/MINGW64.
_CLANG_POSIX="/c/Users/dougc/llvm22/bin/clang.exe"
_CLANG_WIN="C:/Users/dougc/llvm22/bin/clang.exe"

if [[ -n "${CLANG:-}" ]]; then
    :
elif [[ -f "$_CLANG_POSIX" ]]; then
    CLANG="$_CLANG_WIN"
elif command -v clang &>/dev/null; then
    CLANG="clang"
else
    echo "config.sh: ERROR: clang not found at '$_CLANG_WIN' and not on PATH" >&2
    return 1
fi
unset _CLANG_POSIX _CLANG_WIN

# --- Clang++ binary detection (cxx build option) ---
# Only a cxx build requires clang++. CLANGXX in the environment pins an install.
_CLANGXX_POSIX="/c/Users/dougc/llvm22/bin/clang++.exe"
_CLANGXX_WIN="C:/Users/dougc/llvm22/bin/clang++.exe"

if [[ -n "${CLANGXX:-}" ]]; then
    :
elif [[ -f "$_CLANGXX_POSIX" ]]; then
    CLANGXX="$_CLANGXX_WIN"
elif command -v clang++ &>/dev/null; then
    CLANGXX="clang++"
else
    CLANGXX=""
fi

if [[ -z "$CLANGXX" && $cxx -eq 1 ]]; then
    echo "config.sh: ERROR: clang++ not found at '$_CLANGXX_WIN' and not on PATH" >&2
    return 1
fi
unset _CLANGXX_POSIX _CLANGXX_WIN

# --- MinGW sysroot detection ---
# Supplies windows.h, the CRT headers, and the import libraries clang links against
# for the x86_64-w64-mingw32 target.
#
# Ask x86_64-w64-mingw32-gcc where it lives before falling back to /mingw64. Under
# Git for Windows' bash, /mingw64 is Git's own cut-down directory rather than
# MSYS2's, and building against it fails on a missing setjmp.h -- a confusing error
# that says nothing about which sysroot was picked. The gcc lookup lands on the
# MSYS2 installation that actually has the headers.
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

if [[ -z "$MINGW_SYSROOT" ]]; then
    echo "config.sh: ERROR: no MinGW sysroot found. Install MSYS2 and its" \
         "mingw-w64-x86_64-gcc package, or put x86_64-w64-mingw32-gcc on PATH." >&2
    return 1
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
# -Wall -Wextra -Werror is the counterpart of the MSVC build's /W4 /WX: the same
# intent, a different set of diagnostics, so each toolchain reports what the other
# does not. PROJECT_COMPILER_FLAGS carries repository-specific flags.
_COMMON_BASE="-target x86_64-w64-mingw32
    -Wall -Wextra -Werror
    $PROJECT_COMPILER_FLAGS
    -fno-stack-protector
    -D_UNICODE -DUNICODE -D_WIN32 -DWIN32 -D__STDC_WANT_LIB_EXT1__=1
    --sysroot=$MINGW_SYSROOT"

# Define a set of flags for C++. C++20 (-std=c++20) is required to support
# designated initializers.
#
# -x c++: tells clang++ its input is C++. Without it, clang++ still compiles a
# .c file as C++, but warns that treating C input as C++ is deprecated, and
# -Werror turns the warning into an error. "-x c++" applies to every input after
# it, object files included, which clang++ then tries to compile as C++ source.
# A command that lists object files after "-x c++" must add "-x none" before
# them.
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
# -fuse-ld=lld: use LLVM's LLD instead of MinGW's ld. Required for every build.
# Release needs it because -flto produces LLVM bitcode objects that GNU ld
# cannot read. Every build needs it because clang emits native Windows TLS for
# this target, and GNU ld lays a spurious base relocation over the section-
# relative displacement: ASLR then rebases a value that must not be rebased,
# and the first read of any FL_THREAD_LOCAL object faults. That causes a
# segfault for both debug and release builds.
COMMON_LINKER_FLAGS="-fuse-ld=lld -Wl,--stack,1048576"

# The C++ link line adds -static-libstdc++ -static-libgcc: a copy of the runtime
# linked into each C++ binary, rather than one libstdc++-6.dll every one of them
# would otherwise need on the PATH beside it. This sysroot's libstdc++ is the
# posix-threading build, so its exception-handling TLS support (eh_alloc.o,
# emutls.o) pulls in pthread_* symbols regardless -- -Wl,-Bstatic -lwinpthread
# -Wl,-Bdynamic resolves those from the static archive; plain -lwinpthread would
# resolve them from the import library instead, adding a libwinpthread-1.dll
# dependency the static-libstdc++/-libgcc pair above was meant to avoid.
COMMON_LINKER_FLAGS_CXX="$COMMON_LINKER_FLAGS -static-libstdc++ -static-libgcc \
    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic"

# --- FaultLine sources ---
# FaultLine's .c files, for a binary that links them as separate objects rather than
# #including them into its own translation unit.
#
# FL_CORE_SOURCES: the fla_* accessors and the exception module, for a DLL that
# receives injected services. fla_timer_service.c is in it because a suite's
# fl_run_case times the test body through the timer service.
FL_CORE_SOURCES=(
    "$DIR_FL_SRC/fla_log_service.c"
    "$DIR_FL_SRC/fla_memory_service.c"
    "$DIR_FL_SRC/fla_file_service.c"
    "$DIR_FL_SRC/fla_timer_service.c"
    "$DIR_FL_SRC/fl_exception.c"
)

# FL_PLATFORM_SOURCES: the flp_* providers and the arena, for an executable that
# provides the services. fl_threads.c is in it because arena_pool.c stores its
# per-thread shard through the C11 tss_* functions, which MinGW's C runtime does not
# define; fl_threads.c holds the shim fl_threads.h selects when <threads.h> is absent.
FL_PLATFORM_SOURCES=(
    "$DIR_FL_SRC/flp_log_service.c"
    "$DIR_FL_SRC/flp_stream_service.c"
    "$DIR_FL_SRC/flp_timer_service.c"
    "$DIR_FL_SRC/flp_file_service.c"
    "$DIR_FL_SRC/flp_memory_service.c"
    "$DIR_FL_SRC/arena.c"
    "$DIR_FL_SRC/arena_malloc.c"
    "$DIR_FL_SRC/arena_dbg.c"
    "$DIR_FL_SRC/arena_pool.c"
    "$DIR_FL_SRC/digital_search_tree.c"
    "$DIR_FL_SRC/region.c"
    "$DIR_FL_SRC/region_node.c"
    "$DIR_FL_SRC/region_os.c"
    "$DIR_FL_SRC/lock_os.c"
    "$DIR_FL_SRC/fl_threads.c"
)

# compile_fl_sources <dialect> <prefix> <defines> <source>... -- compile FaultLine
# sources into one object each and print the object paths, one per line.
#
# dialect is c or cxx. It has to match the code the objects link with, because the
# exception backend differs between the two.
#
# The prefix keeps two builds of the same file apart. fla_log_service.c is compiled
# once with DLL_BUILD for a DLL and again with FL_EMBEDDED for an executable, and a
# single .o name for both would leave whichever ran last in place for both links. The
# cxx dialect appends _cxx to the prefix for the same reason.
compile_fl_sources() {
    local dialect="$1"; shift
    local prefix="$1"; shift
    local defines="$1"; shift
    local compiler flags src stem out o
    local -a objs=()

    case "$dialect" in
        c)
            compiler="$CLANG"
            flags="$COMMON_COMPILER_FLAGS"
            ;;
        cxx)
            compiler="$CLANGXX"
            flags="$COMMON_COMPILER_FLAGS_CXX"
            prefix="${prefix}_cxx"
            ;;
        *)
            echo "compile_fl_sources: unknown dialect '$dialect'" >&2
            return 1
            ;;
    esac

    for src in "$@"; do
        stem="$(basename "$src" .c)"
        out="$DIR_OUT_OBJ/${prefix}_${stem}.o"
        "$compiler" $flags $defines $PROJECT_INCLUDES -c "$src" -o "$out"
        objs+=("$out")
    done

    for o in "${objs[@]}"; do
        echo "$o"
    done
}

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
