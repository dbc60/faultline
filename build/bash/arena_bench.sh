#!/usr/bin/env bash
# arena_bench.sh - Build arena_bench.exe and arena_bench_nosync.exe.
# Mirrors build/cmd/arena_bench.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Arena Benchmark"

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/arena_bench.ctm"
fi

# Two drivers: one with synchronized-arena support compiled in, and one with
# the project default (FL_ARENA_SYNCHRONIZED undefined).
if [[ $build -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME"

    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_PLATFORM_BUILD -DFL_ARENA_SYNCHRONIZED \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" -I "$DIR_REPO/src" \
        -c "$DIR_REPO/app/arena_bench/arena_bench_main.c" \
        -o "$DIR_OUT_OBJ/arena_bench_main.o" \
        -MJ "$DIR_OUT_OBJ/arena_bench_main.json"

    "$CLANG" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/arena_bench_main.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/arena_bench.exe"

    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME without synchronized-arena support"

    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_PLATFORM_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" -I "$DIR_REPO/src" \
        -c "$DIR_REPO/app/arena_bench/arena_bench_main.c" \
        -o "$DIR_OUT_OBJ/arena_bench_main_nosync.o"

    "$CLANG" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/arena_bench_main_nosync.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/arena_bench_nosync.exe"

    # Keep a copy outside target/, which is what clean removes. A benchmark is
    # only worth reading against its own history, so whatever a run leaves beside
    # the executable -- captured output, a results file later on -- has to outlive
    # a rebuild. The test directories keep their accumulated results the same way.
    mkdir -p "$DIR_REPO/bench_clang"
    cp -f "$DIR_OUT_BIN/arena_bench.exe" "$DIR_REPO/bench_clang/"
    cp -f "$DIR_OUT_BIN/arena_bench_nosync.exe" "$DIR_REPO/bench_clang/"
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/arena_bench.ctm" $?
fi

if [[ $test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run $PROJECT_NAME"
    pushd "$DIR_REPO/bench_clang" > /dev/null
    ./arena_bench.exe
    popd > /dev/null
fi
