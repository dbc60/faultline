# build/bash/iwyu/memory_service.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# Leave empty if the base flags are sufficient.
IWYU_EXTRA_FLAGS="-I $DIR_REPO/third_party"

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/fla_memory_service.c
    src/flp_memory_service.c
)
