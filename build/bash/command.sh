#!/usr/bin/env bash
# command.sh - Build command_tests.dll.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Command" --ctm "command" \
    --src "command_tests.c" --dll "command_tests" \
    "$@"
