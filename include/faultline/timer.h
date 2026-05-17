#ifndef FAULTLINE_TIMER_H_
#define FAULTLINE_TIMER_H_

/**
 * @file timer.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Timer Library - Public API
 * @version 0.2.0
 * @date 2024-12-20
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Unified API for timing operations using different resolution timers:
 * - TimeTimer: Standard time_t resolution (seconds)
 * - WinTimer: High-performance counter (Windows)
 * - TickTimer: CPU time-stamp counter (TSC) for finest resolution
 */
#include <faultline/fl_abbreviated_types.h> // u64, i64
#include <time.h>                           // time_t, struct timespec, struct timeval

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// ============================================================================
// Platform-specific Time Utilities (Windows)
// ============================================================================

#if defined(_MSC_VER)
/**
 * @brief Timeval structure (Windows doesn't define this).
 */
typedef struct timeval {
    long tv_sec;  ///< Seconds
    long tv_usec; ///< Microseconds
} timeval;

/**
 * @brief Timezone structure (obsolete, for compatibility).
 */
struct timezone {
    int tz_minuteswest; ///< Minutes W of Greenwich
    int tz_dsttime;     ///< Type of dst correction
};

/**
 * @brief Sleep for nanoseconds (Windows implementation).
 *
 * @param req Requested sleep time.
 * @param rem Remaining time if interrupted (can be NULL).
 * @return 0 on success, -1 on error.
 */
int nanosleep(const struct timespec *req, struct timespec *rem);

/**
 * @brief Get current time as seconds/microseconds since Unix epoch.
 *
 * @param tp Receives the current time.
 * @param tzp Timezone (obsolete, pass NULL).
 * @return 0 on success.
 */
int gettimeofday(timeval *tp, struct timezone *tzp);
#endif

#if defined(_WIN32)
/**
 * @brief Get the number of 100ns intervals between Windows and Unix epochs.
 *
 * Windows epoch is January 1, 1601. Unix epoch is January 1, 1970.
 * The difference is 116444736000000000 (100ns intervals).
 *
 * @return Number of 100ns intervals between epochs.
 */
u64 get_100ns_intervals_between_epochs(void);

/**
 * @brief The number of 100ns intervals between Windows and Unix epochs.
 */
#define EPOCH_INTERVAL 116444736000000000ULL
#endif

/**
 * @brief Nanoseconds per second constant.
 */
#ifndef NANOSECONDS_PER_SECOND
#define NANOSECONDS_PER_SECOND 1000000000
#endif

// ============================================================================
// TimeTimer: Standard Time Resolution
// ============================================================================

/**
 * @brief Timer using standard time_t (second resolution).
 *
 * Good for long-duration timing where second precision is sufficient.
 */
typedef struct TimeTimer {
    time_t start; ///< Start time in seconds since epoch
    time_t stop;  ///< Stop time in seconds since epoch
} TimeTimer;

/**
 * @brief Start a TimeTimer.
 *
 * @param t The timer to start.
 */
void start(TimeTimer *t);

/**
 * @brief Stop a TimeTimer.
 *
 * @param t The timer to stop.
 */
void stop(TimeTimer *t);

/**
 * @brief Get elapsed time in seconds (integer).
 *
 * @param t The timer to query.
 * @return Elapsed seconds.
 */
time_t elapsed_time(TimeTimer *t);

/**
 * @brief Get elapsed time in seconds (floating point).
 *
 * @param t The timer to query.
 * @return Elapsed seconds as double.
 */
double elapsed_time_seconds(TimeTimer *t);

// ============================================================================
// WinTimer: High-Performance Counter (Windows)
// ============================================================================

#if defined(_WIN32)
/**
 * @brief Timer using Windows QueryPerformanceCounter.
 *
 * Provides high-resolution timing on Windows systems.
 * Typical resolution: microseconds to nanoseconds.
 */
typedef struct WinTimer {
    LARGE_INTEGER start; ///< Start time from performance counter
    LARGE_INTEGER stop;  ///< Stop time from performance counter
} WinTimer;

/**
 * @brief Start a WinTimer.
 *
 * @param t The timer to start.
 */
void start_win(WinTimer *t);

/**
 * @brief Stop a WinTimer.
 *
 * @param t The timer to stop.
 */
void stop_win(WinTimer *t);

/**
 * @brief Get elapsed performance counter ticks.
 *
 * @param t The timer to query.
 * @return Elapsed ticks (use QueryPerformanceFrequency to convert to time).
 */
i64 elapsed_win_ticks(WinTimer *t);

/**
 * @brief Get elapsed time in seconds.
 *
 * @param t The timer to query.
 * @return Elapsed seconds as double.
 */
double elapsed_win_seconds(WinTimer *t);
#endif // _WIN32

// ============================================================================
// TickTimer: CPU Time-Stamp Counter
// ============================================================================

/**
 * @brief Timer using CPU time-stamp counter (TSC).
 *
 * Provides the finest resolution but is sensitive to CPU frequency changes.
 * Use for very short duration measurements or relative comparisons.
 *
 * @note TSC may not be reliable across CPU cores or with dynamic frequency scaling.
 */
typedef struct TickTimer {
    u64 start; ///< Start CPU tick count
    u64 stop;  ///< Stop CPU tick count
} TickTimer;

/**
 * @brief Start a TickTimer.
 *
 * @param t The timer to start.
 */
void start_ticks(TickTimer *t);

/**
 * @brief Stop a TickTimer.
 *
 * @param t The timer to stop.
 */
void stop_ticks(TickTimer *t);

/**
 * @brief Get elapsed CPU ticks.
 *
 * @param t The timer to query.
 * @return Elapsed CPU ticks (raw TSC difference).
 */
u64 elapsed_ticks(TickTimer *t);

/**
 * @brief Get elapsed time in seconds (estimated).
 *
 * Converts TSC ticks to seconds using CPU frequency estimation.
 *
 * @param t The timer to query.
 * @return Estimated elapsed seconds as double.
 * @note Accuracy depends on CPU frequency stability.
 */
double elapsed_tick_seconds(TickTimer *t);

// ============================================================================
// Timer Selection Guide
// ============================================================================

/**
 * Choosing the right timer:
 *
 * - **TimeTimer**: Long durations (seconds to hours), lowest overhead
 *   - Use for: Test suite runtime, overall benchmark duration
 *   - Resolution: ~1 second
 *
 * - **WinTimer**: Medium to short durations (milliseconds to seconds)
 *   - Use for: Individual test timing, function profiling
 *   - Resolution: ~microseconds (Windows QueryPerformanceCounter)
 *   - Platform: Windows only
 *
 * - **TickTimer**: Very short durations (microseconds to milliseconds)
 *   - Use for: Tight loop timing, CPU-level profiling
 *   - Resolution: CPU cycle level
 *   - Caveats: Sensitive to frequency changes, may drift across cores
 *
 * Example:
 * ```c
 * WinTimer timer;
 * start_win(&timer);
 * // ... code to measure ...
 * stop_win(&timer);
 * printf("Elapsed: %.6f seconds\n", elapsed_win_seconds(&timer));
 * ```
 */

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_TIMER_H_
