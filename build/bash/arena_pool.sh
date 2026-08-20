#!/usr/bin/env bash
# arena_pool.sh - Build arena_pool_tests.dll.
# Mirrors build/cmd/arena_pool.cmd.
# Note: MSVC used /wd4456 (hides-previous-local-declaration). If clang emits
# an equivalent warning, add -Wno-shadow or -Wno-shadow-local as needed.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Arena Pool" --ctm "arena_pool" \
    --src "arena_pool_tests.c" --dll "arena_pool_tests" \
    "$@"
