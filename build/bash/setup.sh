#!/usr/bin/env bash
# setup.sh - Derive directory paths, create output directories, handle clean.
# Source this script after options.sh; do not execute it directly.
# Expects options from options.sh to be set ($build, $test, $clean, $cleanall,
# $debug, $release, $verbose, $trace).

# Locate this script's directory (build/bash/) regardless of caller location
_SETUP_SH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Repository root is two directories up from build/bash/
DIR_REPO="$(cd "$_SETUP_SH_DIR/../.." && pwd)"
DIR_INCLUDE="$DIR_REPO/include"
DIR_THIRD_PARTY="$DIR_REPO/third_party"
unset _SETUP_SH_DIR

# If build is not set, enable it when test is requested or when no clean flags are set
if [[ $build -eq 0 ]]; then
    if [[ $test -eq 1 ]]; then
        build=1
    elif [[ $cleanall -eq 0 && $clean -eq 0 ]]; then
        build=1
    fi
fi

# Default to debug if neither debug nor release was requested
if [[ $debug -eq 0 && $release -eq 0 ]]; then
    debug=1
fi

if [[ $release -eq 1 ]]; then
    BUILD_TYPE=release
else
    BUILD_TYPE=debug
fi

# Output directory structure: target/clang/x64/{debug|release}/{bin,lib,obj}
DIR_TARGET="$DIR_REPO/target"
DIR_OUT_BASE="$DIR_TARGET/clang"
DIR_OUT_PLATFORM="$DIR_OUT_BASE/x64"
DIR_OUT_BUILD="$DIR_OUT_PLATFORM/$BUILD_TYPE"
DIR_OUT_OBJ="$DIR_OUT_BUILD/obj"
DIR_OUT_LIB="$DIR_OUT_BUILD/lib"
DIR_OUT_BIN="$DIR_OUT_BUILD/bin"

if [[ $trace -eq 1 ]]; then
    echo "SETUP.SH: directories"
    echo "  DIR_REPO=$DIR_REPO"
    echo "  DIR_INCLUDE=$DIR_INCLUDE"
    echo "  BUILD_TYPE=$BUILD_TYPE"
    echo "  DIR_OUT_BUILD=$DIR_OUT_BUILD"
    echo "  DIR_OUT_BIN=$DIR_OUT_BIN"
    echo "  DIR_OUT_LIB=$DIR_OUT_LIB"
    echo "  DIR_OUT_OBJ=$DIR_OUT_OBJ"
fi

# Handle cleanall: remove the entire clang target tree
if [[ $cleanall -eq 1 ]]; then
    if [[ $verbose -eq 1 && -d "$DIR_OUT_BASE" ]]; then
        echo "Clean All: deleting directory: $DIR_OUT_BASE"
    fi
    rm -rf "$DIR_OUT_BASE"
fi

# Handle clean: remove only the current build-type directory
if [[ $clean -eq 1 ]]; then
    if [[ $verbose -eq 1 && -d "$DIR_OUT_BUILD" ]]; then
        echo "Clean Build: deleting directory: $DIR_OUT_BUILD"
    fi
    rm -rf "$DIR_OUT_BUILD"
fi

# Source compiler/linker configuration when building
if [[ $build -eq 1 ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/config.sh"
fi

# Create output directories when building
if [[ $build -eq 1 ]]; then
    mkdir -p "$DIR_OUT_BIN" "$DIR_OUT_LIB" "$DIR_OUT_OBJ"
fi
