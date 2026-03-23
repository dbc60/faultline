#include <faultline/fl_test.h>                     // FL_TYPE_TEST
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT macros
#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include "fla_memory_service.c"
// this must follow fla_memory_service.c so the macros in fla_memory_service.h are
// defined before the malloc, calloc, realloc, and free functions are declared in
// stdlib.h brought in by malloc_cleanup_config.c
#include "malloc_cleanup_config.c"

typedef struct ConfigTest {
    FLTestCase tc;
    Config    *cfg;
} ConfigTest;

static char const *host     = "example.com";
static char const *user     = "me";
static char const *password = "secret";

FL_SETUP_FN(setup_config) {
    ConfigTest *ct = FL_CONTAINER_OF(tc, ConfigTest, tc);
    ct->cfg        = config_create(host, user, password);
}

FL_CLEANUP_FN(cleanup_config) {
    ConfigTest *ct = FL_CONTAINER_OF(tc, ConfigTest, tc);
    config_destroy(ct->cfg);
}

FL_TYPE_TEST_SETUP_CLEANUP("Create Config", ConfigTest, create, setup_config,
                           cleanup_config) {
    FL_ASSERT_NOT_NULL(t->cfg);
    FL_ASSERT_STR_EQ(t->cfg->host, host);
    FL_ASSERT_STR_EQ(t->cfg->username, user);
    FL_ASSERT_STR_EQ(t->cfg->password, password);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(create)
FL_SUITE_END;

FL_GET_TEST_SUITE("Malloc Config", ts)
