#!/usr/bin/env bash
# region.sh - Build region_tests.dll.
# Mirrors build/cmd/region.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Region" --ctm "region" \
    --src "region_tests.c" --dll "region_tests" \
    "$@"
