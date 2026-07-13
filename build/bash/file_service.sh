#!/usr/bin/env bash
# file_service.sh - Build flp_file_service_tests.dll.
# Mirrors build/cmd/file_service.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "File Service" --ctm "fl_file_service" \
    --src "flp_file_service_tests.c" --dll "flp_file_service_tests" \
    "$@"
