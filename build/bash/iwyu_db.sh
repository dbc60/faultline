#!/usr/bin/env bash
# iwyu_db.sh - Generate a compile database for IWYU analysis of source files.
# Compiles each source file as a standalone TU so IWYU can analyze its includes
# in isolation, rather than through unity-build test TU entry points.
# Output: target/clang/iwyu_compile_commands.json
# Usage: bash build/bash/iwyu_db.sh [debug|release] [verbose]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/options.sh" "$@"
build=1  # ensure setup.sh sources config.sh
source "$SCRIPT_DIR/setup.sh"

_out="$DIR_REPO/target/clang/iwyu_compile_commands.json"
_obj_dir="$DIR_OUT_OBJ/iwyu"
mkdir -p "$_obj_dir"

[[ $verbose -eq 1 ]] && echo "Generating IWYU source database -> $_out"

# Compile each enrolled source file as a standalone TU.
# -Wno-error: unity-build "unused variable" errors don't prevent fragment generation.
# -fsyntax-only: syntax check only, no object output needed.
# -MJ: write the compile_commands.json fragment (generated before compilation).
# Component definitions are sourced from build/bash/iwyu/*.sh; each defines
# IWYU_SOURCES (array of repo-relative paths) and IWYU_EXTRA_FLAGS.
for _comp_def in "$SCRIPT_DIR/iwyu"/*.sh; do
    [[ -f "$_comp_def" ]] || continue
    unset IWYU_SOURCES IWYU_EXTRA_FLAGS
    source "$_comp_def"
    _comp_name="$(basename "$_comp_def" .sh)"
    [[ $verbose -eq 1 ]] && echo "Component: $_comp_name"

    for _src in "${IWYU_SOURCES[@]:-}"; do
        _base="$(basename "$_src" .c)"
        [[ $verbose -eq 1 ]] && echo "  $_src"
        "$CLANG" $COMMON_COMPILER_FLAGS -DDLL_BUILD ${IWYU_EXTRA_FLAGS:-} -Wno-error \
            -I "$DIR_INCLUDE" \
            -fsyntax-only "$DIR_REPO/$_src" \
            -MJ "$_obj_dir/${_base}.json" 2>/dev/null || true
    done
done

# Assemble fragments - same logic as new.sh
{
    printf '[\n'
    _first=1
    for _f in "$_obj_dir"/*.json; do
        [[ -f "$_f" ]] || continue
        [[ $_first -eq 0 ]] && printf ',\n'
        cat "$_f"
        _first=0
    done
    printf '\n]\n'
} | tr -d '\r' | sed -z 's/,\n,\n/,\n/g; s/,\n\n\]/\n\n\]/' > "$_out"

_count="$(grep -c '"file"' "$_out" 2>/dev/null || echo 0)"
echo "IWYU database: $_out ($_count source files)"
unset _out _obj_dir _src _base _f _first _count
