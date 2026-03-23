#!/usr/bin/env bash
# dlist.sh - Build dlist_tests.dll.
# Mirrors build/cmd/dlist.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "DList" --ctm "dlist" \
    --src "dlist_tests.c" --dll "dlist_tests" \
    "$@"
