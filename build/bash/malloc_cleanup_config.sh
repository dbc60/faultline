#!/usr/bin/env bash
# malloc_cleanup_config.sh - Build malloc_cleanup_config_tests.dll.
# Mirrors build/cmd/malloc_cleanup_config.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Malloc Config" --ctm "malloc_cleanup_config" \
    --src "malloc_cleanup_config_tests.c" --dll "malloc_cleanup_config_tests" \
    "$@"
