#!/usr/bin/env bash
# exception_service.sh - Build fl_exception_tests.dll.
# Mirrors build/cmd/exception_service.cmd.
# Note: MSVC used /wd4456 (hides-previous-local-declaration). If clang emits
# an equivalent warning, add -Wno-shadow or -Wno-shadow-local as needed.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Exception Service" --ctm "fl_exception" \
    --src "fl_exception_tests.c" --dll "fl_exception_tests" \
    "$@"
