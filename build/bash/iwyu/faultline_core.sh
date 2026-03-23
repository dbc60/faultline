# build/bash/iwyu/faultline_core.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# faultline_driver.c and faultline_sqlite.c need the private src/ headers and
# the third_party/ SQLite amalgamation header.
IWYU_EXTRA_FLAGS="-I $DIR_REPO/src -I $DIR_REPO/third_party"

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/faultline_context.c
    src/faultline_driver.c
    src/faultline_sqlite.c
)
