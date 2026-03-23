#!/usr/bin/env bash
# timer.sh - Build timer_tests.dll.
# Mirrors build/cmd/timer.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Timer" --ctm "timer" \
    --src "timer_tests.c" --dll "timer_tests" \
    "$@"
