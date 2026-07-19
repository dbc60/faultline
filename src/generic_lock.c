/**
 * @file generic_lock.c
 * @author Douglas Cuthbertson
 * @brief Portable implementation of the fl_lock contract on C11 mtx_t.
 * @version 0.1
 * @date 2026-07-19
 *
 * Serves any platform without a dedicated implementation; fl_threads.h
 * supplies mtx_t from the toolchain's <threads.h> or its compatibility shim.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_lock.h"

#include <faultline/fl_threads.h> // mtx_t, mtx_init, mtx_lock

_Static_assert(sizeof(mtx_t) <= sizeof(FLLock), "FLLock storage is too small for mtx_t");
_Static_assert(_Alignof(mtx_t) <= _Alignof(FLLock),
               "FLLock storage is under-aligned for mtx_t");

bool fl_lock_init(FLLock *lock) {
    return mtx_init((mtx_t *)lock, mtx_plain) == thrd_success;
}

void fl_lock_acquire(FLLock *lock) {
    mtx_lock((mtx_t *)lock);
}

void fl_lock_release(FLLock *lock) {
    mtx_unlock((mtx_t *)lock);
}

void fl_lock_destroy(FLLock *lock) {
    mtx_destroy((mtx_t *)lock);
}
