#!/usr/bin/env bash
# cxx_suite.sh - Build cxx_suite_tests.dll.
# Mirrors build/cmd/cxx_suite.cmd.
#
# Always C++. This suite is not dual-dialect source, so cxx is forced on rather
# than offered: passing it here overrides whatever the caller chose.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "C++ Suite" --ctm "cxx_suite" \
    --src "cxx_suite_tests.cpp" --dll "cxx_suite_tests" \
    cxx "$@"
