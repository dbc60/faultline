#ifndef FAULTLINE_FAULT_H_
#define FAULTLINE_FAULT_H_

/**
 * @file fault.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2025-01-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/buffer.h>          // Buffer API
#include <faultline/fl_result_codes.h> // FLResultCode

#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <faultline/fl_abbreviated_types.h> // i64

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Represents a caught resource-allocation error.
 */
typedef struct Fault {
    i64          index;       ///< the fault-injection index set by the fault injector
    FLResultCode code;        ///< the kind of fault that occurred
    void const  *resource;    ///< a resource passed to a release function. Valid only if
                              ///< code represents an invalid-resource error
    FLExceptionReason reason; ///< the reason for the fault
    char const       *details; ///< optional details about the fault
    char const       *file;    ///< the file in which the fault was detected
    int               line;    ///< the line in the file in which the fault was detected
} Fault;

/**
 * @brief Use FAULT_NO_RESOURCE for faults other than an invalid resource.
 */
#define FAULT_NO_RESOURCE 0

/**
 * @brief Define FaultBuffer, a typed buffer object. This macro defines the following
 * functions:
 *
 *  init_fault_buffer(FaultBuffer *buf, size_t capacity, void *mem)
 *  new_fault_buffer(size_t capacity)
 *  destroy_fault_buffer(FaultBuffer *buf)
 *  fault_put_buffer(FaultBuffer *buf, Fault const *item)
 *  fault_get_buffer(FaultBuffer *buf, size_t index)
 *  fault_count_buffer(FaultBuffer *buf)
 *  fault_clear_buffer(FaultBuffer *buf)
 *  fault_clear_and_release_buffer(FaultBuffer *buf)
 *  fault_allocate_next_free_slot_buffer(FaultBuffer *buf)
 *  fault_copy_buffer(FaultBuffer *dst, FaultBuffer *src)
 */
DEFINE_TYPED_BUFFER(Fault, fault)

/**
 * @brief initialize a new Fault
 *
 * @param caught an allocation error caught during testing
 * @param injection_index the index of the exception point
 * @param code the result code from the test driver
 * @param resource valid only if the error is an invalid address passed to a release
 * function
 * @param reason the reason why an exception was thrown
 * @param details optional details about why an exception was thrown
 * @param file the file in which the exception was thrown
 * @param line the line on which the exception was thrown
 */
static inline void init_fault(Fault *caught, i64 injection_index, FLResultCode code,
                              void const volatile *resource, FLExceptionReason reason,
                              char const *details, char const *file, int line) {
    caught->index    = injection_index;
    caught->code     = code;
    caught->resource = (void *)resource;
    caught->reason   = reason;
    caught->details  = details;
    caught->file     = file;
    caught->line     = line;
}

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_FAULT_H_
