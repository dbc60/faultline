/**
 * @file lock_os.c
 * @author Douglas Cuthbertson
 * @brief platform selection for the fl_lock contract
 *        (fl_lock_init/acquire/release/destroy).
 * @version 0.1
 * @date 2026-07-19
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#if defined(_WIN32) || defined(_WIN64)
#include "win32_lock.c"
#elif defined(__linux__)
// #include "linux_lock.c" if a Linux-specific implementation becomes necessary
#include "generic_lock.c"
#else
#include "generic_lock.c"
#endif
