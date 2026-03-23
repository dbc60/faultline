#!/usr/bin/env bash
# iwyu.sh - Run include-what-you-use on source files.
# Default: target/clang/iwyu_compile_commands.json (individual source files)
# --tests: target/clang/compile_commands.json (unity-build test TUs)
# --fix:   pipe output through fix_includes.py to apply changes automatically
# --group NAME: expand sources from build/bash/iwyu/NAME.sh
# Usage: bash build/bash/iwyu.sh [--fix] [--tests] [--group NAME] [source-file-filter ...]
# Example: bash build/bash/iwyu.sh src/arena.c
# Example: bash build/bash/iwyu.sh --fix src/arena.c
# Example: bash build/bash/iwyu.sh --group arena
# Example: bash build/bash/iwyu.sh --fix --group arena
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"

_iwyu_db="$DIR_REPO/target/clang/iwyu_compile_commands.json"
_test_db="$DIR_REPO/target/clang/compile_commands.json"

_fix=0
_tests=0
_group=""
_pass_args=()
_skip_next=0
for _arg in "$@"; do
    if [[ $_skip_next -eq 1 ]]; then
        _group="$_arg"
        _skip_next=0
        continue
    fi
    case "$_arg" in
        --fix)   _fix=1 ;;
        --tests) _tests=1 ;;
        --group) _skip_next=1 ;;
        *)       _pass_args+=("$_arg") ;;
    esac
done
unset _arg _skip_next

if [[ -n "$_group" ]]; then
    _group_file="$SCRIPT_DIR/iwyu/${_group}.sh"
    if [[ ! -f "$_group_file" ]]; then
        echo "iwyu.sh: group file not found: $_group_file" >&2
        exit 1
    fi
    unset IWYU_SOURCES IWYU_EXTRA_FLAGS
    source "$_group_file"
    for _src in "${IWYU_SOURCES[@]}"; do
        _pass_args+=("$DIR_REPO/$_src")
    done
    unset _src _group_file
fi
unset _group

if [[ $_tests -eq 1 ]]; then
    _ccdb="$_test_db"
    if [[ ! -f "$_ccdb" ]]; then
        echo "iwyu.sh: compile_commands.json not found." >&2
        echo "iwyu.sh: Run 'bash build/bash/new.sh build' first." >&2
        exit 1
    fi
else
    _ccdb="$_iwyu_db"
    if [[ ! -f "$_ccdb" ]]; then
        echo "iwyu.sh: iwyu_compile_commands.json not found." >&2
        echo "iwyu.sh: Run 'bash build/bash/iwyu_db.sh' first." >&2
        exit 1
    fi
fi

_ccdb_win="$(cygpath -m "$_ccdb")"
if [[ $_fix -eq 1 ]]; then
    iwyu_tool.py -p "$_ccdb_win" "${_pass_args[@]}" | fix_includes.py
else
    iwyu_tool.py -p "$_ccdb_win" "${_pass_args[@]}"
fi
unset _fix _tests _pass_args _ccdb _ccdb_win _iwyu_db _test_db
