#ifndef FL_LOCK_H_
#define FL_LOCK_H_

/**
 * @file fl_lock.h
 * @author Douglas Cuthbertson
 * @brief A minimal exclusive-lock contract with per-platform implementations.
 * @version 0.1
 * @date 2026-07-19
 *
 * The lock is embedded by value in structures that need it, so the type must
 * be concrete here without exposing any platform header. FLLock reserves
 * fixed, pointer-aligned storage large enough for every backend; each
 * implementation static-asserts that its real lock type fits. lock_os.c
 * selects the implementation: win32_lock.c on Windows (SRWLOCK), otherwise
 * generic_lock.c (C11 mtx_t via the fl_threads portability header).
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Opaque storage for a platform lock.
 *
 * 64 bytes covers the largest known backend (pthread_mutex_t is 40 bytes on
 * glibc x86-64 and 64 on macOS); the Windows implementation uses only the
 * leading pointer.
 */
typedef union FLLock {
    void         *ptr;
    long long     align_;
    unsigned char storage[64];
} FLLock;

/**
 * @brief Initialize a lock in the unlocked state.
 *
 * @param lock the lock to initialize.
 * @return true on success; false if the platform lock could not be created.
 */
bool fl_lock_init(FLLock *lock);

/**
 * @brief Acquire the lock exclusively, blocking until it is available.
 *
 * The lock is not recursive: re-acquiring it on the owning thread deadlocks.
 *
 * @param lock an initialized lock.
 */
void fl_lock_acquire(FLLock *lock);

/**
 * @brief Release a lock held by the calling thread.
 *
 * @param lock the lock to release.
 */
void fl_lock_release(FLLock *lock);

/**
 * @brief Destroy a lock, releasing any platform resources it holds.
 *
 * The lock must not be held and must not be used again after destruction.
 *
 * @param lock the lock to destroy.
 */
void fl_lock_destroy(FLLock *lock);

#if defined(__cplusplus)
}
#endif

#endif // FL_LOCK_H_
