/**
 * @file malloc_cleanup_config.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-03-07
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#if defined(__clang__) || defined(__GNUC__)
#include <sec_api/string_s.h> // IWYU pragma: keep for strncpy_s
#endif
#include <stddef.h> // IWYU pragma: keep for NULL definition
#include <stdlib.h> // for NULL, free, malloc, size_t, calloc
#include <string.h> // for strlen, strncpy_s

typedef struct Config {
    char *host;
    char *username;
    char *password;
} Config;

void config_destroy(Config *cfg) {
    if (cfg != NULL) {
        if (cfg->host != NULL)
            free(cfg->host);
        if (cfg->username)
            free(cfg->username);
        if (cfg->password)
            free(cfg->password);
        free(cfg);
    }
}

Config *config_create(char const *host, char const *user, char const *pass) {
    Config *cfg      = calloc(1, sizeof(Config));
    size_t  len_host = strlen(host);
    size_t  len_user = strlen(user);
    size_t  len_pass = strlen(pass);

    if (cfg != NULL) {
        cfg->host = malloc(len_host + 1);
        if (cfg->host != NULL) {
            strncpy_s(cfg->host, len_host + 1, host, len_host);
            cfg->username = malloc(len_user + 1);
            if (cfg->username != NULL) {
                strncpy_s(cfg->username, len_user + 1, user, len_user);
                cfg->password = malloc(len_pass + 1);
                if (cfg->password != NULL) {
                    strncpy_s(cfg->password, len_pass + 1, pass, len_pass);
                } else {
                    config_destroy(cfg);
                    return NULL;
                }
            } else {
                config_destroy(cfg);
                return NULL;
            }
        } else {
            config_destroy(cfg);
            return NULL;
        }
    }

    return cfg;
}

/**
 * Three malloc calls.If the second fails, host must be freed.If the third fails, host
 * and username must be freed.This directly tests the most common real-world
 * fault-injection bug : cleanup skipped on partial initialization. FaultLine will
 * discover 3 fault sites and verify no leak at each.
 */
