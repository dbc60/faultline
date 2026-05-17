#ifndef INDEX_H_
#define INDEX_H_
/**
 * @file index.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-10-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#if defined(_MSC_VER) && _MSC_VER >= 1300
#include "index_windows.h" // IWYU pragma: export
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include "index_linux.h" // IWYU pragma: export
#elif defined(__INTEL_COMPILER)
#include "index_intel.h" // IWYU pragma: export
#else
#include "index_generic.h" // IWYU pragma: export
#endif

#endif // INDEX_H_
