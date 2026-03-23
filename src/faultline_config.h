#ifndef FL_CONFIG_H_
#define FL_CONFIG_H_

/**
 * @file faultline_config.h
 * @author Douglas Cuthbertson
 * @brief Configuration system for fault injection testing options.
 * @version 0.1
 * @date 2025-07-24
 *
 * FLConfig provides control over fault injection testing behavior,
 * including site selection, execution modes, and reporting options.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include <faultline/fault_site.h>           // FaultSite, FaultSiteBuffer
#include <faultline/fl_abbreviated_types.h> // i64
#include <stdbool.h>                        // bool

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Execution mode for fault injection testing
 */
typedef enum {
    FL_MODE_DISCOVERY_ONLY, ///< Only run discovery pass, no fault injection
    FL_MODE_TARGETED,       ///< Test only enabled fault sites
    FL_MODE_EXHAUSTIVE      ///< Test all discovered fault sites (default)
} FLExecutionMode;

/**
 * @brief Progress reporting level
 */
typedef enum {
    FL_PROGRESS_NONE,    ///< No progress reporting
    FL_PROGRESS_BASIC,   ///< Basic progress indicators
    FL_PROGRESS_DETAILED ///< Detailed progress with site information
} FLProgressLevel;

/**
 * @brief Configuration options for fault injection testing
 */
typedef struct FLConfig {
    bool            initialized;     ///< whether config has been initialized
    FLExecutionMode execution_mode;  ///< how to execute fault injection tests
    FLProgressLevel progress_level;  ///< level of progress reporting
    bool            collect_metrics; ///< whether to collect detailed metrics
    bool            show_site_info;  ///< whether to display fault site information
    i64             max_sites;       ///< maximum number of sites to test (0 = no limit)
    i64             start_site_id;   ///< start testing from this site ID (1-based)
    i64 end_site_id; ///< stop testing at this site ID (1-based, 0 = no limit)
} FLConfig;

/**
 * @brief Initialize FLConfig with default values
 *
 * @param config the configuration object to initialize
 */
static inline void faultline_config_init(FLConfig *config) {
    config->initialized     = true;
    config->execution_mode  = FL_MODE_EXHAUSTIVE;
    config->progress_level  = FL_PROGRESS_BASIC;
    config->collect_metrics = true;
    config->show_site_info  = false;
    config->max_sites       = 0; // no limit
    config->start_site_id   = 1; // start from first site
    config->end_site_id     = 0; // no end limit
}

/**
 * @brief Check if config has been initialized
 *
 * @param config the configuration object
 * @return true if initialized, false otherwise
 */
static inline bool faultline_config_is_initialized(FLConfig *config) {
    return config->initialized;
}

/**
 * @brief Set execution mode
 *
 * @param config the configuration object
 * @param mode the execution mode to set
 */
static inline void faultline_config_set_execution_mode(FLConfig       *config,
                                                       FLExecutionMode mode) {
    config->execution_mode = mode;
}

/**
 * @brief Get execution mode
 *
 * @param config the configuration object
 * @return the current execution mode
 */
static inline FLExecutionMode faultline_config_get_execution_mode(FLConfig *config) {
    return config->execution_mode;
}

/**
 * @brief Set progress reporting level
 *
 * @param config the configuration object
 * @param level the progress level to set
 */
static inline void faultline_config_set_progress_level(FLConfig       *config,
                                                       FLProgressLevel level) {
    config->progress_level = level;
}

/**
 * @brief Get progress reporting level
 *
 * @param config the configuration object
 * @return the current progress level
 */
static inline FLProgressLevel faultline_config_get_progress_level(FLConfig *config) {
    return config->progress_level;
}

/**
 * @brief Enable or disable metrics collection
 *
 * @param config the configuration object
 * @param enabled whether to collect metrics
 */
static inline void faultline_config_set_collect_metrics(FLConfig *config, bool enabled) {
    config->collect_metrics = enabled;
}

/**
 * @brief Check if metrics collection is enabled
 *
 * @param config the configuration object
 * @return true if metrics collection is enabled
 */
static inline bool faultline_config_is_collect_metrics(FLConfig *config) {
    return config->collect_metrics;
}

/**
 * @brief Enable or disable site information display
 *
 * @param config the configuration object
 * @param enabled whether to show site information
 */
static inline void faultline_config_set_show_site_info(FLConfig *config, bool enabled) {
    config->show_site_info = enabled;
}

/**
 * @brief Check if site information display is enabled
 *
 * @param config the configuration object
 * @return true if site information display is enabled
 */
static inline bool faultline_config_is_show_site_info(FLConfig *config) {
    return config->show_site_info;
}

/**
 * @brief Set the maximum number of sites to test
 *
 * @param config the configuration object
 * @param max_sites maximum number of sites (0 = no limit)
 */
static inline void faultline_config_set_max_sites(FLConfig *config, i64 max_sites) {
    config->max_sites = max_sites;
}

/**
 * @brief Get the maximum number of sites to test
 *
 * @param config the configuration object
 * @return maximum number of sites (0 = no limit)
 */
static inline i64 faultline_config_get_max_sites(FLConfig *config) {
    return config->max_sites;
}

/**
 * @brief Set the site ID range for testing
 *
 * @param config the configuration object
 * @param start_id starting site ID (1-based)
 * @param end_id ending site ID (1-based, 0 = no limit)
 */
static inline void faultline_config_set_site_range(FLConfig *config, i64 start_id,
                                                   i64 end_id) {
    config->start_site_id = start_id;
    config->end_site_id   = end_id;
}

/**
 * @brief Get the starting site ID
 *
 * @param config the configuration object
 * @return starting site ID (1-based)
 */
static inline i64 faultline_config_get_start_site_id(FLConfig *config) {
    return config->start_site_id;
}

/**
 * @brief Get the ending site ID
 *
 * @param config the configuration object
 * @return ending site ID (1-based, 0 = no limit)
 */
static inline i64 faultline_config_get_end_site_id(FLConfig *config) {
    return config->end_site_id;
}

/**
 * @brief Check if a site ID should be tested based on configuration
 *
 * @param config the configuration object
 * @param site_id the site ID to check (1-based)
 * @return true if the site should be tested
 */
static inline bool faultline_config_should_test_site(FLConfig *config, i64 site_id) {
    if (site_id < config->start_site_id) {
        return false;
    }

    if (config->end_site_id > 0 && site_id > config->end_site_id) {
        return false;
    }

    return true;
}

/**
 * @brief Get a string representation of the execution mode
 *
 * @param mode the execution mode
 * @return string representation
 */
static inline char const *faultline_execution_mode_to_string(FLExecutionMode mode) {
    switch (mode) {
    case FL_MODE_DISCOVERY_ONLY:
        return "discovery-only";
    case FL_MODE_TARGETED:
        return "targeted";
    case FL_MODE_EXHAUSTIVE:
        return "exhaustive";
    default:
        return "unknown";
    }
}

/**
 * @brief Get a string representation of the progress level
 *
 * @param level the progress level
 * @return string representation
 */
static inline char const *faultline_progress_level_to_string(FLProgressLevel level) {
    switch (level) {
    case FL_PROGRESS_NONE:
        return "none";
    case FL_PROGRESS_BASIC:
        return "basic";
    case FL_PROGRESS_DETAILED:
        return "detailed";
    default:
        return "unknown";
    }
}

#if defined(__cplusplus)
}
#endif

#endif // FL_CONFIG_H_
