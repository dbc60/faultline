# build/bash/iwyu/exception_service.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# Leave empty if the base flags are sufficient.
IWYU_EXTRA_FLAGS=""

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/fl_exception_service.c
    src/fla_exception_service.c
    src/flp_exception_service.c
)
