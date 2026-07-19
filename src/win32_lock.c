/**
 * @file win32_lock.c
 * @author Douglas Cuthbertson
 * @brief Windows implementation of the fl_lock contract on SRWLOCK.
 * @version 0.1
 * @date 2026-07-19
 *
 * SRWLOCK is a single pointer, initializes by zeroing, cannot fail to
 * initialize, and needs no teardown, which makes it considerably cheaper than
 * a kernel- or CRT-backed mutex for uncontended exclusive locking.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_lock.h"

#include <faultline/fl_macros.h> // FL_UNUSED

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // SRWLOCK, AcquireSRWLockExclusive

_Static_assert(sizeof(SRWLOCK) <= sizeof(FLLock),
               "FLLock storage is too small for SRWLOCK");

bool fl_lock_init(FLLock *lock) {
    InitializeSRWLock((PSRWLOCK)lock);
    return true;
}

void fl_lock_acquire(FLLock *lock) {
    AcquireSRWLockExclusive((PSRWLOCK)lock);
}

void fl_lock_release(FLLock *lock) {
    ReleaseSRWLockExclusive((PSRWLOCK)lock);
}

void fl_lock_destroy(FLLock *lock) {
    // SRWLOCKs hold no resources beyond their storage.
    FL_UNUSED(lock);
}
