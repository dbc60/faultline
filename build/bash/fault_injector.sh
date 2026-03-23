#!/usr/bin/env bash
# fault_injector.sh - Build fault_injector_tests.dll.
# Mirrors build/cmd/fault_injector.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Fault Injector" --ctm "fault_injector" \
    --src "fault_injector_tests.c" --dll "fault_injector_tests" \
    "$@"
