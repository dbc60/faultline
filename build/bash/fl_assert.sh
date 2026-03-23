#!/usr/bin/env bash
# fl_assert.sh - Build fl_assert_tests.dll.
# Tests all FL_ASSERT* macros in include/fl_exception_service_assert.h.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Assert Macros" --ctm "fl_assert" \
    --src "fl_assert_tests.c" --dll "fl_assert_tests" \
    "$@"
