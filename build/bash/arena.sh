#!/usr/bin/env bash
# arena.sh - Build arena_tests.dll.
# Mirrors build/cmd/arena.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Arena" --ctm "arena" \
    --src "arena_tests.c" --dll "arena_tests" \
    "$@"
