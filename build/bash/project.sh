#!/usr/bin/env bash
# project.sh - Repository-specific settings for the clang build. options.sh, setup.sh,
# config.sh and build_test_dll.sh read these.
# Sourced by setup.sh.
# Expects the directories from setup.sh and $trace from options.sh.

# FaultLine source file location used by config.sh to define the FL_*_SOURCES lists
DIR_FL_SRC="$DIR_SRC"

# Include paths
PROJECT_INCLUDES="-I $DIR_INCLUDE -I $DIR_THIRD_PARTY -I $DIR_THIRD_PARTY/fnv"

# Compiler flags appended to config.sh's -Wall -Wextra -Werror.
#
# Enable warning when a format string is not a literal. This helps ensure that
# the LOG_* family of macros and the FL_THROW_DETAILS macro both take a format
# string from their caller.
PROJECT_COMPILER_FLAGS="-Wformat-nonliteral"

# The test driver for exercising test suites, and the script that builds it
# when it is missing.
FL_TEST_DRIVER="${FL_TEST_DRIVER:-$DIR_OUT_BIN/faultline.exe}"
FL_TEST_DRIVER_SCRIPT="faultline.sh"

# The script that compiles $DIR_OUT_OBJ/sqlite3.o, for a suite built with --sqlite
SQLITE_OBJ_SCRIPT="faultline.sh"

if [[ $trace -eq 1 ]]; then
    echo "PROJECT.SH: settings"
    echo "  DIR_FL_SRC=$DIR_FL_SRC"
    echo "  PROJECT_INCLUDES=$PROJECT_INCLUDES"
    echo "  PROJECT_COMPILER_FLAGS=$PROJECT_COMPILER_FLAGS"
    echo "  FL_TEST_DRIVER=$FL_TEST_DRIVER"
    echo "  FL_TEST_DRIVER_SCRIPT=$FL_TEST_DRIVER_SCRIPT"
    echo "  SQLITE_OBJ_SCRIPT=$SQLITE_OBJ_SCRIPT"
fi
