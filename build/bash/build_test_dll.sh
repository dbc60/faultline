#!/usr/bin/env bash
# build_test_dll.sh - Shared helper: compile one test DLL and optionally run it.
# Called by individual component scripts; not intended to be invoked directly.
#
# Usage:
#   build_test_dll.sh --name NAME --ctm CTM --src SRC --dll DLL [OPTIONS] [BUILD_ARGS...]
#
# Required options:
#   --name NAME      Display name for progress messages (e.g. "Arena")
#   --ctm  CTM       Stem for the ctime metrics file   (e.g. "arena" -> metrics/arena.ctm)
#   --src  SRC       Source file under src/             (e.g. "arena_tests.c")
#   --dll  DLL       Output DLL stem, without extension (e.g. "arena_tests")
#
# Optional options:
#   --extra-src SRC  Extra source under src/ compiled and linked in before the main
#                    source; may be repeated for multiple extras
#
# BUILD_ARGS are forwarded verbatim to options.sh:
#   build, debug, release, clean, cleanall, test, verbose, trace, timed
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---------------------------------------------------------------------------
# Parse helper-specific flags; collect remaining args for options.sh
# ---------------------------------------------------------------------------
_project_name=""
_ctm_name=""
_src_file=""
_dll_name=""
_extra_srcs=()
_build_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)      _project_name="$2"; shift 2 ;;
        --ctm)       _ctm_name="$2";     shift 2 ;;
        --src)       _src_file="$2";     shift 2 ;;
        --dll)       _dll_name="$2";     shift 2 ;;
        --extra-src) _extra_srcs+=("$2"); shift 2 ;;
        *)           _build_args+=("$1"); shift ;;
    esac
done

source "$SCRIPT_DIR/options.sh" "${_build_args[@]+"${_build_args[@]}"}"
source "$SCRIPT_DIR/setup.sh"

# ---------------------------------------------------------------------------
# Metrics (begin)
# ---------------------------------------------------------------------------
if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/${_ctm_name}.ctm"
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if [[ $build -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Build ${_project_name} test suite"

    # Compile any extra sources first (e.g. fl_threads.c for log/memory service tests)
    _extra_objs=()
    for _esrc in "${_extra_srcs[@]+"${_extra_srcs[@]}"}"; do
        _estem="${_esrc%.c}"
        "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
            -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
            -I "$DIR_THIRD_PARTY/fnv" \
            -c "$DIR_REPO/src/${_esrc}" \
            -o "$DIR_OUT_OBJ/${_estem}.o" \
            -MJ "$DIR_OUT_OBJ/${_estem}.json"
        _extra_objs+=("$DIR_OUT_OBJ/${_estem}.o")
    done

    # Compile the main test source
    _src_stem="${_src_file%.c}"
    "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD \
        -I "$DIR_INCLUDE" -I "$DIR_THIRD_PARTY" \
        -I "$DIR_THIRD_PARTY/fnv" \
        -c "$DIR_REPO/src/${_src_file}" \
        -o "$DIR_OUT_OBJ/${_src_stem}.o" \
        -MJ "$DIR_OUT_OBJ/${_src_stem}.json"

    # Link
    "$CLANG" -target x86_64-w64-mingw32 -shared \
        "$DIR_OUT_OBJ/${_src_stem}.o" \
        "${_extra_objs[@]+"${_extra_objs[@]}"}" \
        $COMMON_LINKER_FLAGS \
        -o "$DIR_OUT_BIN/${_dll_name}.dll"
fi

# ---------------------------------------------------------------------------
# Metrics (end)
# ---------------------------------------------------------------------------
if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/${_ctm_name}.ctm" $?
fi

# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------
if [[ $test -eq 1 ]]; then
    if [[ ! -f "$DIR_OUT_BIN/faultline.exe" ]]; then
        _driver_args=(build)
        [[ $release -eq 1 ]] && _driver_args+=(release)
        bash "$SCRIPT_DIR/faultline.sh" "${_driver_args[@]}"
    fi
    [[ $verbose -eq 1 ]] && echo "Run ${_project_name} unit tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    ./faultline.exe run "${_dll_name}.dll"
    faultline.exe show results --limit 1
    faultline.exe show results --failures
    popd > /dev/null
fi
