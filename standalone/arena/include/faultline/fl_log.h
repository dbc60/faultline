#ifndef FL_LOG_H_
#define FL_LOG_H_

/**
 * @file fl_log.h
 * @brief Standalone override — all LOG_* macros are no-ops.
 */

#define LOG_VERBOSE(component, fmt, ...) ((void)0)
#define LOG_DEBUG(component, fmt, ...)   ((void)0)
#define LOG_INFO(component, fmt, ...)    ((void)0)
#define LOG_WARN(component, fmt, ...)    ((void)0)
#define LOG_ERROR(component, fmt, ...)   ((void)0)

#endif // FL_LOG_H_
