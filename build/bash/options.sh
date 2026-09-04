#!/usr/bin/env bash
# options.sh - Parse build CLI arguments and set flag variables.
# Source this script; do not execute it directly.
# Usage: source options.sh [args...]
#
# Available options:
#   build     - compile and link (default when no clean flag given)
#   debug     - debug build: -O0 -g -DDEBUG (default)
#   release   - optimized build: -O2 -g -flto
#   clean     - delete target/clang/x64/{debug or release}/
#   cleanall  - delete all of target/clang/
#   test      - build then run tests (implies build)
#   junit     - when combined with test, write JUnit XML results to test_clang/junit.xml
#   verbose   - print each command before running it
#   trace     - print all option variable values after parsing
#   timed     - collect build timing metrics via ctime.exe
#   cxx       - build test suites as C++ (COMMON_COMPILER_FLAGS_CXX and the
#               FL_EXC_BACKEND_CXX exception backend, driving clang++ instead
#               of clang) instead of the C/setjmp default. Drivers, hosts,
#               libraries and examples are always C: a suite contains its own
#               exceptions behind fl_run_case, so one C driver runs suites of
#               either dialect. Mirrors options.cmd's cxx.

# Initialize all flags to 0
build=0
debug=0
release=0
clean=0
cleanall=0
test=0
junit=0
verbose=0
trace=0
timed=0
cxx=0

# Parse arguments
for _opt in "$@"; do
    case "$_opt" in
        build)    build=1 ;;
        debug)    debug=1 ;;
        release)  release=1 ;;
        clean)    clean=1 ;;
        cleanall) cleanall=1 ;;
        test)     test=1 ;;
        junit)    junit=1 ;;
        verbose)  verbose=1 ;;
        trace)    trace=1 ;;
        timed)    timed=1 ;;
        cxx)      cxx=1 ;;
        *)
            echo "options.sh: unknown option '$_opt'" >&2
            ;;
    esac
done
unset _opt

if [[ $trace -eq 1 ]]; then
    echo "OPTIONS.SH: options set"
    echo "  build=$build"
    echo "  debug=$debug"
    echo "  release=$release"
    echo "  clean=$clean"
    echo "  cleanall=$cleanall"
    echo "  test=$test"
    echo "  junit=$junit"
    echo "  verbose=$verbose"
    echo "  trace=$trace"
    echo "  timed=$timed"
    echo "  cxx=$cxx"
fi
