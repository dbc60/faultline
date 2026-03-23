#!/usr/bin/env bash
# buffer.sh - Build buffer_tests.dll.
# Mirrors build/cmd/buffer.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Buffer" --ctm "buffer" \
    --src "buffer_tests.c" --dll "buffer_tests" \
    "$@"
