#ifndef FAULT_INJECTOR_RESOURCE_H_
#define FAULT_INJECTOR_RESOURCE_H_

/**
 * @file fault_injector_resource.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2025-12-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <fnv/FNV64.h> // FNV64Block

#include <faultline/set.h> // Set implementation (needs full definition)
#include <faultline/fl_log.h>          // LOG_DEBUG

#include <faultline/fl_macros.h> // FL_UNUSED
#include <faultline/fl_try.h>    // FL_THROW

typedef struct InjectorResource {
    void const *value;
    char const *file;
    int         line;
} InjectorResource;

DEFINE_TYPED_SET(InjectorResource, injector_resource)

static inline void injector_resource_init(InjectorResource    *ir,
                                          void const volatile *value, char const *file,
                                          int line) {
    ir->value = (void *)value;
    ir->file  = file;
    ir->line  = line;
}

static inline u64 injector_resource_hash(Set const *set, void const *value) {
    FL_UNUSED(set);
    u64                     hash = 0;
    InjectorResource const *ir   = value;
    LOG_DEBUG("FAULT INJECTOR", "hash: SetEntry=0x%p", value);
    LOG_DEBUG("FAULT INJECTOR", "hash: InjectorResource=0x%p", ir);
    LOG_DEBUG("FAULT INJECTOR", "hash: IR value=0x%p", ir->value);
    int result = FNV64block(&ir->value, (long)(sizeof ir->value), (uint8_t *)&hash);

    LOG_VERBOSE("FAULT INJECTOR", "%d: 0x%p hashed to %zu", result, ir->value, hash);
    if (result != fnvSuccess) {
        FL_THROW(fl_internal_error);
    }

    return hash;
}

static inline bool injector_resource_compare(Set const *set, void const *a,
                                             void const *b) {
    FL_UNUSED(set);
    InjectorResource const *ira = a;
    InjectorResource const *irb = b;
    LOG_VERBOSE("FAULT INJECTOR", "comparing 0x%p == 0x%p", ira->value, irb->value);
    return ira->value == irb->value;
}

#endif // FAULT_INJECTOR_RESOURCE_H_
