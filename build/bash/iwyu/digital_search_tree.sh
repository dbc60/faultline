# build/bash/iwyu/digital_search_tree.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# Leave empty if the base flags are sufficient.
IWYU_EXTRA_FLAGS=""

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/digital_search_tree.c
)
