#ifndef FAULTLINE_ARENA_H_
#define FAULTLINE_ARENA_H_

/**
 * @file arena.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Memory Arena Library - Public API
 * @version 0.2.0
 * @date 2007-09-10
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * High-performance memory arena allocator with region-based management.
 * Supports fast allocation and bulk deallocation with minimal overhead.
 */
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

#if defined(__cplusplus)
extern "C" {
#endif

#define ARENA_ALIGNED_ALLOC_THROW(AR, ALIGN, SZ) \
    arena_aligned_alloc_throw((AR), (ALIGN), (SZ), __FILE__, __LINE__)
#define ARENA_CALLOC_THROW(AR, COUNT, BYTES) \
    arena_calloc_throw((AR), (COUNT), (BYTES), __FILE__, __LINE__)
#define ARENA_FREE_THROW(AR, PTR) arena_free_throw((AR), (PTR), __FILE__, __LINE__)
#define ARENA_FREE_POINTER_THROW(AR, PTR) \
    arena_free_pointer_throw((AR), (PTR), __FILE__, __LINE__)
#define ARENA_MALLOC_THROW(AR, BYTES) \
    arena_malloc_throw((AR), (BYTES), __FILE__, __LINE__)
#define ARENA_REALLOC_THROW(AR, PTR, BYTES) \
    arena_realloc_throw((AR), (PTR), (BYTES), __FILE__, __LINE__)

/**
 * @brief Opaque handle to a memory arena.
 *
 * The Arena structure manages memory allocation through a combination of
 * binning strategies for different size classes. Implementation details
 * are hidden to maintain API stability.
 */
typedef struct Arena Arena;

// ============================================================================
// Arena Lifecycle
// ============================================================================

/**
 * @brief Allocate a new Arena in a block of memory large enough to satisfy
 * the initial commit size and with additional memory reserved for future use.
 *
 * @param commit The number of bytes to commit beyond the arena structure size.
 *               This value will be rounded up to the system granularity (64KB on
 * Windows). Use SIZE_ macros for convenience (e.g., MEBI(1) for 1 MiB).
 * @param reserve The number of additional granularity blocks to reserve.
 *                Each block is typically 64KB on Windows. Use 0 for no extra
 * reservation.
 * @return The address of the newly allocated Arena, or throws arena_out_of_memory.
 * @throw arena_out_of_memory when memory cannot be allocated.
 *
 * @note The total reserved space is: ALIGN_UP(sizeof(Arena) + commit, granularity)
 *       + (reserve * granularity)
 * @note Example: new_arena(MEBI(1), 0) creates a 1 MiB committed arena with no extra
 * reservation.
 * @note An arena created this way performs no internal synchronization; all
 *       calls against it must come from one thread, or the caller must
 *       serialize them. Define FL_ARENA_SYNCHRONIZED and use new_shared_arena
 *       for an arena that is safe to use from multiple threads concurrently.
 */
Arena *new_arena(size_t commit, unsigned int reserve);

#ifdef FL_ARENA_SYNCHRONIZED
/**
 * @brief Allocate a new Arena whose mutating entry points serialize internally,
 * making it safe for concurrent use from multiple threads.
 *
 * Takes the same parameters as new_arena. Each mutating call (malloc, free,
 * calloc, realloc, aligned_alloc, free_pointer, set_footprint_limit) acquires
 * the arena's lock for its duration; composite operations such as realloc are
 * atomic as a unit. The lock is released even when a call unwinds by throwing.
 *
 * Serialization has a fixed per-call cost, so prefer new_arena plus one arena
 * per thread when the usage pattern allows it, and reserve shared arenas for
 * state that genuinely must be visible across threads.
 *
 * Available only when FL_ARENA_SYNCHRONIZED is defined, and the definition
 * must be consistent project-wide: it selects which allocator entry points are
 * compiled, so define it (or not) for every translation unit in the build,
 * typically on the compiler command line. Projects that never share an arena
 * across threads simply leave it undefined and pay no synchronization cost of
 * any kind; defining it later is a recompile, not a source change.
 *
 * @param commit The number of bytes to commit beyond the arena structure size.
 * @param reserve The number of additional granularity blocks to reserve.
 * @return The address of the newly allocated Arena, or throws arena_out_of_memory.
 * @throw arena_out_of_memory when memory cannot be allocated.
 * @throw fl_internal_error when the arena's lock cannot be initialized.
 */
Arena *new_shared_arena(size_t commit, unsigned int reserve);
#endif // FL_ARENA_SYNCHRONIZED

/**
 * @brief Release all memory allocated by an Arena and set the pointer to NULL.
 *
 * @param ap A pointer to the address of the Arena to be released.
 *           The pointer will be set to NULL after release.
 * @note This frees all memory regions managed by the arena.
 */
void release_arena(Arena **ap);

// ============================================================================
// Memory Allocation
// ============================================================================

////////////////////////////////////////////////////
// Exception Throwing Memory Management Functions //
////////////////////////////////////////////////////

/**
 * @brief Allocate a block of memory aligned to alignment bytes.
 *
 * alignment must be a power of 2 and at least 1. size must be a multiple of alignment.
 * Throws arena_out_of_memory if the allocation fails or fl_invalid_value if the
 * alignment is not a power of 2.
 *
 * @param alignment the required alignment in bytes (must be a power of 2)
 * @param size the number of bytes to allocate
 * @param file
 * @param line
 * @return a pointer to size bytes of memory aligned to alignment bytes
 * @throw arena_out_of_memory, fl_invalid_value
 */
void *arena_aligned_alloc_throw(Arena *arena, size_t alignment, size_t size,
                                char const *file, int line);
void *arena_calloc_throw(Arena *arena, size_t count, size_t size, char const *file,
                         int line);

/**
 * @brief Release a previously allocated block of memory back to the arena.
 *
 * @param arena The arena that owns the memory.
 * @param mem The memory block to release.
 * @param file Source file name (typically __FILE__).
 * @param line Source line number (typically __LINE__).
 * @throw fl_invalid_address if mem is not a valid allocation from this arena.
 *
 * @note Released memory is returned to appropriate bins for reuse.
 * @note File/line are used for debugging.
 */
void arena_free_throw(Arena *arena, void *ptr, char const *file, int line);
void arena_free_pointer_throw(Arena *arena, void **ptr, char const *file, int line);

/**
 * @brief Free a block from a thread that does not own the arena.
 *
 * Pushes the block onto the owning arena's remote-free queue with a single
 * lock-free atomic operation; the owning thread reclaims queued blocks
 * through the normal free path at its next allocation. Safe to call from any
 * thread and on any arena, synchronized or not. Performs no validation, so
 * the caller must pass a live allocation from this arena.
 *
 * @param arena The arena that owns the block (see arena_owner).
 * @param mem The block to free.
 */
void arena_free_remote(Arena *arena, void *mem);

/**
 * @brief Return the arena a block was allocated from.
 *
 * The owner is recorded in the block's header at allocation time and remains
 * readable from any thread for as long as the block stays allocated.
 *
 * @param mem A live allocation from some arena.
 * @return The owning arena.
 */
Arena *arena_owner(void const *mem);
void  *arena_malloc_throw(Arena *arena, size_t request, char const *file, int line);
void  *arena_realloc_throw(Arena *arena, void *mem, size_t size, char const *file,
                           int line);

/**
 * @brief Allocate a block of memory from the arena.
 *
 * The arena uses a multi-strategy allocation algorithm:
 * 1. For small requests (<= 992 bytes):
 *    a. Try to find a bin with an exact fit
 *    b. Try the fast node (recently split chunk)
 *    c. Try a larger small bin, splitting if needed
 *    d. Try large bins, splitting if needed
 * 2. For large requests (>= 1024 bytes):
 *    a. Search large bins for best fit
 * 3. If all bins fail:
 *    a. Allocate from top (wilderness chunk)
 *    b. Expand arena from OS if needed
 *
 * @param arena The arena to allocate from.
 * @param request The number of bytes to allocate.
 * @param file Source file name (typically __FILE__).
 * @param line Source line number (typically __LINE__).
 * @return A pointer to allocated memory, aligned to 16-byte boundary.
 * @throw arena_out_of_memory if allocation fails.
 * @throw fl_internal_error if arena integrity is compromised.
 *
 * @note Allocations are always 16-byte aligned on 64-bit systems.
 * @note File/line are used for debugging and tracking allocations.
 */
void *arena_malloc_throw(Arena *arena, size_t request, char const *file, int line);

// ============================================================================
// Mid-Level Arena Inspection APIs
// ============================================================================

/**
 * @brief Get the current memory footprint of the arena.
 *
 * The footprint represents the total amount of memory committed (in use)
 * by the arena, including both allocated and free memory in bins.
 *
 * @param arena The arena to query.
 * @return The number of bytes currently committed from the OS.
 * @note This includes overhead for arena structures and free chunks.
 */
size_t arena_footprint(Arena const *arena);

/**
 * @brief Get the maximum memory footprint ever reached by the arena.
 *
 * This is a high-water mark tracking the largest amount of memory
 * the arena has ever committed during its lifetime.
 *
 * @param arena The arena to query.
 * @return The maximum number of bytes ever committed.
 * @note Useful for tuning initial commit/reserve sizes.
 */
size_t arena_max_footprint(Arena const *arena);

/**
 * @brief Get the number of currently active allocations in the arena.
 *
 * This counter increments with each arena_malloc_throw() call and decrements
 * with each arena_free_throw() call.
 */
size_t arena_allocation_count(Arena const *arena);

/**
 * @brief Get the allocated size of a memory block.
 *
 * Returns the actual size of the chunk containing the user allocation,
 * which may be larger than the requested size due to alignment and
 * minimum chunk size requirements.
 *
 * @param mem A pointer to an allocated memory block.
 * @return The size of the chunk containing this allocation.
 * @note The returned size includes chunk metadata overhead.
 * @note Returns 0 if mem is NULL.
 */
size_t arena_allocation_size(void *mem);

/**
 * @brief Check if an address is a valid allocation from this arena.
 *
 * This function validates that:
 * 1. The address falls within one of the arena's memory regions
 * 2. The address is within the allocated/allocatable space
 *
 * @param arena The arena to check against.
 * @param mem The address to validate.
 * @return true if mem is a valid allocation from this arena, false otherwise.
 * @note This checks region bounds, not whether the specific address is
 *       currently allocated vs. freed.
 */
bool arena_is_valid_allocation(Arena *arena, void const *mem);

// ============================================================================
// System Information
// ============================================================================

void init_mparams(void);

/**
 * @brief Set the maximum memory footprint the arena may use.
 *
 * The limit is rounded up to the system allocation granularity. Pass 0 to
 * remove any previously set limit (the default is unlimited growth).
 *
 * Once set to a non-zero value, any allocation that would require growing the
 * arena beyond the limit throws arena_out_of_memory instead.
 *
 * @param arena The arena to configure.
 * @param limit Maximum committed bytes, or 0 for unlimited.
 * @throw arena_out_of_memory
 */
void arena_set_footprint_limit(Arena *arena, size_t limit);

/**
 * @brief Get the operating system's page size.
 *
 * @return The page size in bytes (typically 4096 bytes on most systems).
 */
size_t arena_page_size(void);

/**
 * @brief Get the operating system's memory allocation granularity.
 *
 * Memory regions are allocated in multiples of this size.
 *
 * @return The allocation granularity in bytes (typically 65536 bytes on Windows).
 */
size_t arena_granularity(void);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_ARENA_H_
