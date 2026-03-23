#!/usr/bin/env bash
# new.sh - Build all components in dependency order, then assemble compile_commands.json.
# Mirrors build/cmd/new.cmd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
source "$SCRIPT_DIR/setup.sh"

# Build a forwarding arg list: strip 'test' (we run tests ourselves at the end),
# but translate 'test' -> 'build' so sub-scripts know to compile.
_forward_args=()
_has_test=0
for _a in "$@"; do
    case "$_a" in
        test)
            _has_test=1
            _forward_args+=(build)
            ;;
        clean|cleanall)
            # Clean is handled at the new.sh level via setup.sh; skip for sub-components
            ;;
        *)
            _forward_args+=("$_a")
            ;;
    esac
done
unset _a

if [[ $timed -eq 1 ]]; then
    mkdir -p "$DIR_REPO/metrics/clang"
    ctime.exe -begin "$DIR_REPO/metrics/clang/all.ctm"
fi

# Build all components in dependency order (mirrors new.cmd order)
bash "$SCRIPT_DIR/exception_service.sh"     "${_forward_args[@]}"
bash "$SCRIPT_DIR/but_driver.sh"            "${_forward_args[@]}"
bash "$SCRIPT_DIR/dlist.sh"                 "${_forward_args[@]}"
bash "$SCRIPT_DIR/log_service.sh"           "${_forward_args[@]}"
bash "$SCRIPT_DIR/bits.sh"                  "${_forward_args[@]}"
bash "$SCRIPT_DIR/region.sh"                "${_forward_args[@]}"
bash "$SCRIPT_DIR/chunk.sh"                 "${_forward_args[@]}"
bash "$SCRIPT_DIR/digital_search_tree.sh"   "${_forward_args[@]}"
bash "$SCRIPT_DIR/index.sh"                 "${_forward_args[@]}"
bash "$SCRIPT_DIR/arena.sh"                 "${_forward_args[@]}"
bash "$SCRIPT_DIR/buffer.sh"                "${_forward_args[@]}"
bash "$SCRIPT_DIR/fnv.sh"                   "${_forward_args[@]}"
bash "$SCRIPT_DIR/math.sh"                  "${_forward_args[@]}"
bash "$SCRIPT_DIR/set.sh"                   "${_forward_args[@]}"
bash "$SCRIPT_DIR/timer.sh"                 "${_forward_args[@]}"
bash "$SCRIPT_DIR/fault_injector.sh"        "${_forward_args[@]}"
bash "$SCRIPT_DIR/fl_assert.sh"             "${_forward_args[@]}"
bash "$SCRIPT_DIR/command.sh"               "${_forward_args[@]}"
bash "$SCRIPT_DIR/faultline.sh"             "${_forward_args[@]}"
bash "$SCRIPT_DIR/malloc_cleanup_config.sh" "${_forward_args[@]}"

# Copy build artifacts to test/ directory (mirrors new.cmd behavior)
if [[ $build -eq 1 ]]; then
    mkdir -p "$DIR_REPO/test_clang"
    cp -f "$DIR_OUT_BIN"/*.exe "$DIR_REPO/test_clang/" 2>/dev/null || true
    cp -f "$DIR_OUT_BIN"/*.dll "$DIR_REPO/test_clang/" 2>/dev/null || true
fi

if [[ $timed -eq 1 ]]; then
    ctime.exe -end "$DIR_REPO/metrics/clang/all.ctm" 0
fi

# Assemble compile_commands.json from all per-TU .json fragments
if [[ $build -eq 1 ]]; then
    _ccdb="$DIR_REPO/target/clang/compile_commands.json"
    [[ $verbose -eq 1 ]] && echo "Assembling compile_commands.json -> $_ccdb"
    {
        printf '[\n'
        _first=1
        for _f in "$DIR_REPO/target/clang"/x64/*/obj/*.json; do
            [[ -f "$_f" ]] || continue
            [[ $_first -eq 0 ]] && printf ',\n'
            cat "$_f"
            _first=0
        done
        printf '\n]\n'
    } | tr -d '\r' | sed -z 's/,\n,\n/,\n/g; s/,\n\n\]/\n\n\]/' > "$_ccdb"
    unset _ccdb _first _f
fi

# Run all tests
if [[ $_has_test -eq 1 ]]; then
    [[ $verbose -eq 1 ]] && echo "Run all unit tests"
    _junit_opt=()
    [[ $junit -eq 1 ]] && _junit_opt=(--junit-xml junit.xml)
    pushd "$DIR_REPO/test_clang/" > /dev/null
    ./faultline.exe run "${_junit_opt[@]}" \
        fl_exception_tests.dll \
        but_tests.dll \
        dlist_tests.dll \
        fl_log_service_tests.dll \
        bits_tests.dll \
        region_tests.dll \
        chunk_tests.dll \
        digital_search_tree_tests.dll \
        index_generic_tests.dll \
        index_windows_tests.dll \
        arena_tests.dll \
        buffer_tests.dll \
        fnv_tests.dll \
        math_tests.dll \
        set_tests.dll \
        timer_tests.dll \
        fault_injector_tests.dll \
        fl_assert_tests.dll \
        command_tests.dll \
        faultline_tests.dll \
        malloc_cleanup_config_tests.dll
    ./faultline.exe show results --limit 21
    popd > /dev/null
fi

unset _forward_args _has_test _junit_opt
