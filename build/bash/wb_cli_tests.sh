#!/usr/bin/env bash
# wb_cli_tests.sh - Build wb_cli_tests.dll.
# Mirrors build/cmd/wb_cli_tests.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Worldbuilder CLI" \
    --src "wb_cli_tests.c" --dll "wb_cli_tests" \
    --fl-core \
    "$@"
