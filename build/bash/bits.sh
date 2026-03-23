#!/usr/bin/env bash
# bits.sh - Build bits_tests.dll.
# Mirrors build/cmd/bits.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Bits" --ctm "bits" \
    --src "bits_tests.c" --dll "bits_tests" \
    "$@"
