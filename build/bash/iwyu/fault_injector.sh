# build/bash/iwyu/fault_injector.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# Leave empty if the base flags are sufficient.
IWYU_EXTRA_FLAGS="-I $DIR_REPO/third_party -I $DIR_REPO/third_party/fnv"

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/fault_injector.c
)
