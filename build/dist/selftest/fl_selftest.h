#ifndef FL_SELFTEST_H_
#define FL_SELFTEST_H_

/**
 * @file fl_selftest.h
 * @author Douglas Cuthbertson
 * @brief Shared CHECK / SECTION macros for distribution package smoke tests.
 *
 * Each smoke test is a single-TU binary, so the static g_failures counter is
 * per-translation-unit by design.
 * @version 0.1
 * @date 2026-05-30
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

#define SECTION(name) fprintf(stdout, "  %s\n", (name))

#endif /* FL_SELFTEST_H_ */
