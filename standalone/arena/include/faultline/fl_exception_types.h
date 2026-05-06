#ifndef FL_EXCEPTION_TYPES_H_
#define FL_EXCEPTION_TYPES_H_

/**
 * @file fl_exception_types.h
 * @brief Standalone override — full FLExceptionEnvironment for setjmp/longjmp.
 */
#include <setjmp.h> // jmp_buf
#include <stdint.h> // uint32_t

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef FL_MAX_DETAILS_LENGTH
#define FL_MAX_DETAILS_LENGTH 512
#endif

typedef enum {
    FL_ENTERED,
    FL_THROWN,
    FL_HANDLED,
} FLExceptionState;

typedef char const *FLExceptionReason;

typedef struct FLExceptionEnvironment {
    jmp_buf                        jmp;
    struct FLExceptionEnvironment *next;
    FLExceptionReason volatile reason;
    char const *volatile details;
    char const *volatile file;
    uint32_t volatile line;
    FLExceptionState volatile state;
} FLExceptionEnvironment;

#define FL_EXCEPTION_HANDLER_FN(name)                                   \
    void name(void *ctx, FLExceptionReason reason, char const *details, \
              char const *file, int line)
typedef FL_EXCEPTION_HANDLER_FN(fl_exception_handler_fn);

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_TYPES_H_
