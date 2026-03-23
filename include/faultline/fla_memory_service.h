#ifndef FLA_MEMORY_SERVICE_H_
#define FLA_MEMORY_SERVICE_H_

/**
 * @file fla_memory_service.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-02-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_memory_service.h> // FLMemoryService
#include <faultline/fl_macros.h>         // FL_DECL_SPEC
#include <stdlib.h> // IWYU pragma: keep - must be processed before the macros below shadow malloc/free/calloc/realloc

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Thread-local memory service.
 *
 * Each DLL gets its own copy of this variable (static linkage). The driver sets it via
 * fl_set_memory_service() after loading the DLL so its write field points to the
 * driver's implementation.
 */
extern FLMemoryService g_fla_memory_service;

/**
 * @brief Set the memory service for an application.
 *
 * @param svc Address of a platform-owned memory service
 * @param size the size of the memory service in bytes
 */
FL_DECL_SPEC FLA_SET_MEMORY_SERVICE_FN(fla_set_memory_service);

// macros to redirect C memory management functions to the service's implementation
#define aligned_alloc(AL, SZ) \
    g_fla_memory_service.fl_aligned_alloc((AL), (SZ), __FILE__, __LINE__)
#define calloc(NMEMB, SZ) \
    g_fla_memory_service.fl_calloc((NMEMB), (SZ), __FILE__, __LINE__)
#define free(PTR)        g_fla_memory_service.fl_free((PTR), __FILE__, __LINE__)
#define malloc(SZ)       g_fla_memory_service.fl_malloc((SZ), __FILE__, __LINE__)
#define realloc(PTR, SZ) g_fla_memory_service.fl_realloc((PTR), (SZ), __FILE__, __LINE__)

#if defined(__cplusplus)
}
#endif

#endif // FLA_MEMORY_SERVICE_H_
