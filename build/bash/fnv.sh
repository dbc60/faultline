#!/usr/bin/env bash
# fnv.sh - Build fnv_tests.dll.
# Mirrors build/cmd/fnv.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "FNV" --ctm "fnv" \
    --src "fnv_tests.c" --dll "fnv_tests" \
    "$@"
