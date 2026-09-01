#ifndef ATOMIC_H_
#define ATOMIC_H_

/**
 * @file atomic.h
 * @author Douglas Cuthbertson
 * @brief define some useful atomic types.
 * @version 0.1
 * @date 2025-01-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_abbreviated_types.h>

#if defined(__cplusplus)
#include <atomic> // std::atomic
// C declares these at file scope; C++ has them in std. The atomic_load and
// atomic_fetch_add family reaches std by argument-dependent lookup on a
// std::atomic operand, so those need nothing here. A memory order constant and
// the atomic_int typedef have no such operand, so name them.
using std::atomic_int;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
#else
#include <stdatomic.h> // _Atomic, atomic_load, atomic_fetch_add, memory_order_*
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Declare an atomic object of type T.
 *
 * C has the _Atomic type specifier; C++ has the std::atomic class template, and
 * MSVC's <stdatomic.h> is C++23-only. Only declarations need the distinction: the
 * C11 atomic_load / atomic_store / atomic_fetch_add / atomic_compare_exchange_*
 * calls resolve on a std::atomic operand by argument-dependent lookup, so call
 * sites read the same under either dialect.
 *
 *   FL_ATOMIC(size_t) footprint;
 *   FL_ATOMIC(void *) remote_free_head;
 */
#if defined(__cplusplus)
#define FL_ATOMIC(T) std::atomic<T>
#else
#define FL_ATOMIC(T) _Atomic(T)
#endif

// This was atomic_char_ptr, but note that C11 reserves names beginning with atomic_
// followed by a lowercase letter
typedef FL_ATOMIC(char *) AtomicCharPtr;

#if defined(__cplusplus)
}
#endif

#endif // ATOMIC_H_
