/**
 * @file fla_timer_service.c
 * @author Douglas Cuthbertson
 * @brief The application side of the timer service
 * @version 0.1
 * @date 2026-06-18
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fla_timer_service.h> // for fla_set_timer_service declaration
#include <faultline/fl_timer_service.h>  // for FLTimestamp, FLTimerService, etc.
#include <faultline/fl_macros.h>         // for FL_UNUSED, FL_DECL_SPEC
#include <stdio.h>                       // for fprintf, stderr
#include <stdlib.h>                      // for abort

static FL_TIMER_NOW_FN(default_now) {
    fprintf(stderr, "Timer service is uninitialized - no now function provided\n");
    fflush(stderr);
    abort();
}

static FL_TIMER_ELAPSED_FN(default_elapsed_seconds) {
    FL_UNUSED(start);
    FL_UNUSED(end);
    fprintf(stderr,
            "Timer service is uninitialized - no elapsed_seconds function provided\n");
    fflush(stderr);
    abort();
}

FLTimerService g_fla_timer_service
    = {.now = default_now, .elapsed_seconds = default_elapsed_seconds};

FL_DECL_SPEC FLA_SET_TIMER_SERVICE_FN(fla_set_timer_service) {
    if (size < sizeof(FLTimerService)) {
        fprintf(stderr, "invalid timer service - expected %zu bytes, received %zu\n",
                sizeof(FLTimerService), size);
        fflush(stderr);
        abort();
    }

    g_fla_timer_service = *svc;
}
