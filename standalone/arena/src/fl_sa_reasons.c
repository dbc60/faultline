/**
 * @file fl_sa_reasons.c
 * @author Douglas Cuthbertson
 * @brief Shared exception reason strings for the standalone arena.
 *
 * These definitions satisfy the extern declarations in fl_exception_service.h
 * and fl_exception_service_assert.h.  arena_out_of_memory is defined in arena.c.
 * @version 0.1
 * @date 2026-05-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_types.h>

FLExceptionReason fl_expected_failure   = "expected failure";
FLExceptionReason fl_test_exception     = "test exception";
FLExceptionReason fl_not_implemented    = "not implemented";
FLExceptionReason fl_invalid_value      = "invalid value";
FLExceptionReason fl_internal_error     = "internal error";
FLExceptionReason fl_invalid_address    = "invalid address";
FLExceptionReason fl_unexpected_failure = "unexpected failure";
