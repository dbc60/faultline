#!/usr/bin/env bash
# math.sh - Build math_tests.dll.
# Mirrors build/cmd/math.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Math" --ctm "math" \
    --src "math_tests.c" --dll "math_tests" \
    "$@"
