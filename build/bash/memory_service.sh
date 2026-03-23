#!/usr/bin/env bash
# memory_service.sh - Build flp_memory_service_tests.dll.
# Mirrors build/cmd/memory_service.cmd.
# Note: MSVC used /wd4456 (hides-previous-local-declaration). If clang emits
# an equivalent warning, add -Wno-shadow or -Wno-shadow-local as needed.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Memory Service" --ctm "fl_memory_service" \
    --src "flp_memory_service_tests.c" --dll "flp_memory_service_tests" \
    "$@"
