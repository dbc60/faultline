#ifndef FLP_MODULE_SERVICE_H_
#define FLP_MODULE_SERVICE_H_

/**
 * @file flp_module_service.h
 * @author Douglas Cuthbertson
 * @brief Platform provider of OS module loading and suite service injection.
 * @version 0.1
 * @date 2026-07-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <platform_api.h> // FLModule, FL_MODULE_*_FN, FL_INJECT_SERVICES_FN

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct FLFaultMemoryContext FLFaultMemoryContext;

/**
 * @brief Bind the fault-injecting memory context that flp_inject_services()
 * pushes into suites. Call once at host setup, before any suite is injected.
 */
#define FLP_MODULE_SERVICE_INIT_FN(name) void name(FLFaultMemoryContext *fault_ctx)
typedef FLP_MODULE_SERVICE_INIT_FN(flp_module_service_init_fn);
FLP_MODULE_SERVICE_INIT_FN(flp_module_service_init);

/**
 * @brief Platform-side implementation to load a module by path. Returns NULL on
 * failure.
 */
FL_MODULE_LOAD_FN(flp_load_module);

/**
 * @brief Platform-side implementation to resolve an exported symbol from a loaded
 * module. Returns NULL if the symbol is not exported.
 */
FL_MODULE_SYMBOL_FN(flp_resolve_symbol);

/**
 * @brief Platform-side implementation to unload a module. A NULL module is ignored.
 */
FL_MODULE_UNLOAD_FN(flp_unload_module);

/**
 * @brief Inject the platform's services into a loaded suite through the
 * fla_set_*_service symbols it exports. Returns false if the suite is missing a
 * required service (exception handling).
 */
FL_INJECT_SERVICES_FN(flp_inject_services);

#if defined(__cplusplus)
}
#endif

#endif // FLP_MODULE_SERVICE_H_
