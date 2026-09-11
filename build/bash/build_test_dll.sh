#!/usr/bin/env bash
# build_test_dll.sh - Shared helper: compile one test DLL and optionally run it.
# Called by individual component scripts; not intended to be invoked directly.
#
# Usage:
#   build_test_dll.sh --name NAME --src SRC --dll DLL [OPTIONS] [BUILD_ARGS...]
#
# Required options:
#   --name NAME      Display name for progress messages (e.g. "Arena")
#   --src  SRC       Source file under src/, .c or .cpp (e.g. "arena_tests.c")
#   --dll  DLL       Output DLL stem, without extension (e.g. "arena_tests")
#
# Optional options:
#   --ctm  CTM       Stem for the ctime metrics file (e.g. "arena" ->
#                    metrics/clang/arena.ctm); defaults to DLL
#   --extra-src SRC  Extra source under src/ compiled and linked in before the main
#                    source; may be repeated for multiple extras
#   --fl-core        Link FaultLine's FL_CORE_SOURCES (config.sh), for a suite that
#                    does not #include them itself
#   --sqlite         Link sqlite3.o, built by project.sh's SQLITE_OBJ_SCRIPT when
#                    missing
#
# BUILD_ARGS are forwarded verbatim to options.sh.
#
# With test, the suite runs from the output bin directory through project.sh's
# FL_TEST_DRIVER, built first by FL_TEST_DRIVER_SCRIPT when one is named and the
# driver is missing.
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
_fl_core=0
_needs_sqlite=0
_build_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)      _project_name="$2";  shift 2 ;;
        --ctm)       _ctm_name="$2";      shift 2 ;;
        --src)       _src_file="$2";      shift 2 ;;
        --dll)       _dll_name="$2";      shift 2 ;;
        --extra-src) _extra_srcs+=("$2"); shift 2 ;;
        --fl-core)   _fl_core=1;          shift ;;
        --sqlite)    _needs_sqlite=1;     shift ;;
        *)           _build_args+=("$1"); shift ;;
    esac
done

source "$SCRIPT_DIR/options.sh" "${_build_args[@]+"${_build_args[@]}"}"
source "$SCRIPT_DIR/setup.sh"

_ctm_name="${_ctm_name:-$_dll_name}"

# Options for a nested build of a missing prerequisite: same build type, no clean,
# no test.
_nested_args=(build)
[[ $release -eq 1 ]] && _nested_args+=(release)
[[ $verbose -eq 1 ]] && _nested_args+=(verbose)

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

    # The source under test is always first-party, so it takes whichever
    # dialect cxx picked. Every object linked with it takes the same dialect.
    _dialect=c
    _compiler="$CLANG"
    _compiler_flags="$COMMON_COMPILER_FLAGS"
    _linker_flags="$COMMON_LINKER_FLAGS"
    if [[ $cxx -eq 1 ]]; then
        _dialect=cxx
        _compiler="$CLANGXX"
        _compiler_flags="$COMMON_COMPILER_FLAGS_CXX"
        _linker_flags="$COMMON_LINKER_FLAGS_CXX"
    fi

    _link_objs=()

    # Link the shared sqlite3.o, building it first when missing
    if [[ $_needs_sqlite -eq 1 ]]; then
        if [[ ! -f "$DIR_OUT_OBJ/sqlite3.o" ]]; then
            if [[ -z "$SQLITE_OBJ_SCRIPT" ]]; then
                echo "build_test_dll.sh: ERROR: --sqlite needs SQLITE_OBJ_SCRIPT" \
                     "in project.sh" >&2
                exit 1
            fi
            bash "$SCRIPT_DIR/$SQLITE_OBJ_SCRIPT" "${_nested_args[@]}"
        fi
        _link_objs+=("$DIR_OUT_OBJ/sqlite3.o")
    fi

    # A suite is a DLL the driver loads, so it takes FaultLine's core side and
    # DLL_BUILD
    if [[ $_fl_core -eq 1 ]]; then
        mapfile -t _core_objs < <(compile_fl_sources "$_dialect" core -DDLL_BUILD \
            "${FL_CORE_SOURCES[@]}")
        _link_objs+=("${_core_objs[@]}")
    fi

    # Compile any extra sources first (e.g. fl_threads.c for log/memory service tests)
    for _esrc in "${_extra_srcs[@]+"${_extra_srcs[@]}"}"; do
        _estem="${_esrc%.*}"
        "$_compiler" $_compiler_flags -DDLL_BUILD $PROJECT_INCLUDES \
            -c "$DIR_SRC/${_esrc}" \
            -o "$DIR_OUT_OBJ/${_estem}.o" \
            -MJ "$DIR_OUT_OBJ/${_estem}.json"
        _link_objs+=("$DIR_OUT_OBJ/${_estem}.o")
    done

    # Compile the main test source
    _src_stem="${_src_file%.*}"
    "$_compiler" $_compiler_flags -DDLL_BUILD $PROJECT_INCLUDES \
        -c "$DIR_SRC/${_src_file}" \
        -o "$DIR_OUT_OBJ/${_src_stem}.o" \
        -MJ "$DIR_OUT_OBJ/${_src_stem}.json"

    # Link
    "$_compiler" -target x86_64-w64-mingw32 --sysroot="$MINGW_SYSROOT" -shared \
        "$DIR_OUT_OBJ/${_src_stem}.o" \
        "${_link_objs[@]+"${_link_objs[@]}"}" \
        $_linker_flags \
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
    if [[ -n "$FL_TEST_DRIVER_SCRIPT" ]] && ! command -v "$FL_TEST_DRIVER" &>/dev/null; then
        bash "$SCRIPT_DIR/$FL_TEST_DRIVER_SCRIPT" "${_nested_args[@]}"
    fi
    [[ $verbose -eq 1 ]] && echo "Run ${_project_name} unit tests"
    pushd "$DIR_OUT_BIN" > /dev/null
    "$FL_TEST_DRIVER" run "${_dll_name}.dll"
    "$FL_TEST_DRIVER" show results --limit 1
    "$FL_TEST_DRIVER" show results --failures
    popd > /dev/null
fi
