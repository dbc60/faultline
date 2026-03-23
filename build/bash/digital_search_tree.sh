#!/usr/bin/env bash
# digital_search_tree.sh - Build digital_search_tree_tests.dll.
# Mirrors build/cmd/digital_search_tree.cmd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_test_dll.sh" \
    --name "Digital Search Tree" --ctm "digital_search_tree" \
    --src "digital_search_tree_tests.c" --dll "digital_search_tree_tests" \
    "$@"
