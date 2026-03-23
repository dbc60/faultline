#!/usr/bin/env bash
# log_service.sh - Build fl_log_service_tests.dll.
# Mirrors build/cmd/log_service.cmd.
# Note: MSVC used /wd4456 (hides-previous-local-declaration). If clang emits
# an equivalent warning, add -Wno-shadow or -Wno-shadow-local as needed.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Log Service" --ctm "fl_log_service" \
    --extra-src "fl_threads.c" \
    --src "fl_log_service_tests.c" --dll "fl_log_service_tests" \
    "$@"
