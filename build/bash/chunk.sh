#!/usr/bin/env bash
# chunk.sh - Build chunk_tests.dll.
# Mirrors build/cmd/chunk.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Chunk" --ctm "chunk" \
    --src "chunk_tests.c" --dll "chunk_tests" \
    "$@"
