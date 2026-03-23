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

#include <stdatomic.h>

#if defined(__cplusplus)
extern "C" {
#endif

// This was atomic_char_ptr, but note that C11 reserves names beginning with atomic_
// followed by a lowercase letter
typedef _Atomic(char *) AtomicCharPtr;

#if defined(__cplusplus)
}
#endif

#endif // ATOMIC_H_
