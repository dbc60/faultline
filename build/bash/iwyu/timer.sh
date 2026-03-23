# build/bash/iwyu/timer.sh
# Component definition for IWYU analysis - sourced by iwyu_db.sh.
# Extra flags appended after COMMON_COMPILER_FLAGS -DDLL_BUILD -Wno-error.
# Leave empty if the base flags are sufficient.
IWYU_EXTRA_FLAGS="-include faultline/fla_exception_service.h"

# Source files to analyze, relative to repo root.
IWYU_SOURCES=(
    src/tick_timer.c
    src/time_timer.c
    src/win_timer.c
)
