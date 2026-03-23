#!/usr/bin/env bash
# set.sh - Build set_tests.dll.
# Mirrors build/cmd/set.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Set" --ctm "set" \
    --src "set_tests.c" --dll "set_tests" \
    "$@"
