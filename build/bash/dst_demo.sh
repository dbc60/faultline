#!/usr/bin/env bash
# dst_demo.sh - Build dst_demo.wasm, the digital search tree for a browser demo.
#
# This script has no build/cmd counterpart: only clang targets WebAssembly. It uses
# wasi-sdk's clang rather than the CLANG that config.sh selects, so it does not source
# options.sh, setup.sh or config.sh, and all.sh does not call it.
#
# Usage:
#   bash build/bash/dst_demo.sh [test]
#
# test runs dst_demo_check.mjs under Node after the build.
set -euo pipefail

# WASI_SDK_PATH in the environment pins an install. Otherwise take the primary location.
_WASI_SDK_DEFAULT="/c/Users/dougc/wasi-sdk-34.0-x86_64-windows"
WASI_SDK_PATH="${WASI_SDK_PATH:-$_WASI_SDK_DEFAULT}"

if [[ ! -f "$WASI_SDK_PATH/bin/clang.exe" ]]; then
    echo "dst_demo.sh: ERROR: clang.exe not found in '$WASI_SDK_PATH/bin'" >&2
    exit 1
fi

# clang.exe takes Windows paths.
DIR_REPO="$(cygpath -m "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)")"
DIR_DEMO="$DIR_REPO/app/dst_demo"
DIR_OUT="$DIR_REPO/target/clang/wasm32"

mkdir -p "$DIR_OUT"

# stddef.h is force-included because fl_abbreviated_types.h uses wchar_t, which the
# Windows headers otherwise provide. wasm_include precedes the sysroot so its setjmp.h
# replaces wasi-libc's.
"$WASI_SDK_PATH/bin/clang.exe" --target=wasm32-wasip1 -std=c17 -O2 -Wall \
    -include stddef.h \
    -I"$DIR_DEMO/wasm_include" -I"$DIR_REPO/include" -I"$DIR_REPO/src" \
    -nostartfiles -Wl,--no-entry -Wl,--gc-sections -Wl,--strip-all \
    "$DIR_DEMO/dst_demo.c" -o "$DIR_OUT/dst_demo.wasm"

if [[ "${1:-}" == "test" ]]; then
    node "$DIR_DEMO/dst_demo_check.mjs" "$DIR_OUT/dst_demo.wasm"
fi
