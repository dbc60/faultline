#!/usr/bin/env bash
# arena_trace.sh - Build arena_trace.exe and write a JSON trace for each scenario.
# Mirrors build/cmd/arena_trace.cmd.
#
# The traces land in target/clang/wasm32/traces, where dst_demo.sh collects them for the
# page. The compiler changes nothing in a trace, so either script's output serves.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

PROJECT_NAME="Arena Trace"
DIR_TRACES="$DIR_REPO/target/clang/wasm32/traces"

if [[ $build -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Build $PROJECT_NAME"

    # Always C. cxx selects the dialect of a test suite, and this is not one.
    "$CLANG" $COMMON_COMPILER_FLAGS -DFL_PLATFORM_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" -I "$DIR_REPO/src" \
        -c "$DIR_REPO/app/arena_trace/arena_trace_main.c" \
        -o "$DIR_OUT_OBJ/arena_trace_main.o" \
        -MJ "$DIR_OUT_OBJ/arena_trace_main.json"

    "$CLANG" -target x86_64-w64-mingw32 \
        "$DIR_OUT_OBJ/arena_trace_main.o" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/arena_trace.exe"
fi

mkdir -p "$DIR_TRACES"

# Run from the output directory, which is where arena_trace.log belongs.
pushd "$DIR_OUT_BIN" > /dev/null
for _scenario in "$DIR_REPO"/app/arena_trace/scenarios/*.txt; do
    ./arena_trace.exe < "$_scenario" > "$DIR_TRACES/$(basename "$_scenario" .txt).json"
done
popd > /dev/null
