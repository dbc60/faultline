#ifndef FL_MEMORY_H_
#define FL_MEMORY_H_

/**
 * @file fl_memory.h
 * @brief
 */

#if defined(FL_PLATFORM_BUILD)
#include <flp_memory_service.h> // IWYU pragma: export
#define FL_MALLOC(SZ)            flp_malloc((SZ), __FILE__, __LINE__)
#define FL_FREE(PTR)             flp_free((PTR), __FILE__, __LINE__)
#define FL_CALLOC(N, SZ)         flp_calloc((N), (SZ), __FILE__, __LINE__)
#define FL_REALLOC(PTR, SZ)      flp_realloc((PTR), (SZ), __FILE__, __LINE__)
#define FL_ALIGNED_ALLOC(AL, SZ) flp_aligned_alloc((AL), (SZ), __FILE__, __LINE__)
#else                                     // Application/DLL build
#include <faultline/fla_memory_service.h> // IWYU pragma: export
// fla_memory_service.h already redefines malloc/free/calloc/realloc/aligned_alloc
// as macros that route through g_fla_memory_service.
#define FL_MALLOC(SZ)            malloc(SZ)
#define FL_FREE(PTR)             free(PTR)
#define FL_CALLOC(N, SZ)         calloc((N), (SZ))
#define FL_REALLOC(PTR, SZ)      realloc((PTR), (SZ))
#define FL_ALIGNED_ALLOC(AL, SZ) aligned_alloc((AL), (SZ))
#endif // FL_PLATFORM_BUILD

#endif // FL_MEMORY_H_
