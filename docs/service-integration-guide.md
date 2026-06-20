# Service Integration Guide
This guide covers how to wire up FaultLine's runtime services — **exception handling**, **logging**, **memory**, and **timing** — when building a host that loads and drives service consumers.

> To bring the service source into another repository in the first place — and
> keep it updated as packages change — see the
> [Service Distribution Guide](service-distribution.md).

Two axes are in play (see CLAUDE.md → "Two architectural axes"):

- **Portability:** **core** (portable contracts + implementations) vs. **platform** (OS-specific implementations + host). The reusable unit is each service's `fl_*_service.h` contract plus its portable core implementation.
- **Provider / consumer:** the **host** provides services (`flp_`); a **consumer** receives them (`fla_`). A consumer is usually a test-suite DLL, but the host's own core is a consumer of the services it installs into itself.

## Background: The Service Pattern

Each service has three layers:

| Prefix | Role        | What it contains                                                                         |
| ------ | ----------- | ---------------------------------------------------------------------------------------- |
| `fl_`  | Shared      | Type definitions, function-pointer typedefs, `FLFooService` struct                       |
| `flp_` | Platform    | Concrete implementations, lifecycle functions, `flp_init_*_service()` injector           |
| `fla_` | Consumer    | Global service variable (`g_fla_*_service`), default stubs, `fla_set_*_service()` setter |

The injection sequence at runtime is always the same:

1. Platform calls its `flp_*_init()` lifecycle functions to set up the service.
2. Platform loads the application DLL (`LoadLibrary`).
3. Platform resolves the DLL's setter with `GetProcAddress` using the well-known symbol name (e.g., `FLA_SET_EXCEPTION_SERVICE_STR`).
4. Platform calls `flp_init_*_service(fla_set)`, which fills `g_fla_*_service` inside the DLL.
5. Application code now calls through `g_fla_*_service` function pointers transparently.

## 1. Exception Handling Service

The exception service is **optional** in general. A platform that does not use `FL_TRY`/`FL_CATCH` and does not inject the memory service has no need to require it from an application DLL. The existing `but` and `faultline` drivers treat it as required and refuse to run a DLL that does not export `fla_set_exception_service`, but that is a policy choice made by those drivers, not a constraint of the service design.

### Platform side

**Headers to include:**

```c
#include <flp_exception_service.h>           // FL_TRY / FL_CATCH macros, flp_push/pop/throw,
                                             //   flp_init_exception_service
// Pulled in automatically by the above:
//   <faultline/fl_exception_service.h>      // FLExceptionService, fla_set_exception_service_fn,
//                                           //   fl_expected_failure, fl_invalid_value, ...
//   <faultline/fl_exception_types.h>        // FLExceptionEnvironment, FLExceptionState, ...
```

**Source files to compile and link:**

```
src/flp_exception_service.c   — platform TLS stack + push/pop/throw + flp_init_exception_service
src/fl_exception_service.c    — shared reason-string constants (fl_expected_failure, etc.)
```

**Compile flag:** none required, but build the platform target with `/DFL_BUILD_DRIVER` (a compiler flag, not an in-source `#define`) if you also use the `fl_memory.h` / `fl_log.h` selector headers (see §3 and the Memory section).

**Initialization (no explicit init call needed):** The platform TLS stack initializes lazily; just ensure the platform wraps its top-level execution in `FL_TRY` / `FL_END_TRY` so there is always a frame on the stack when a test throws.

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_exception_service.h>  // fla_set_exception_service_fn,
                                             //   FLA_SET_EXCEPTION_SERVICE_STR

fla_set_exception_service_fn *fla_set_exc =
    (fla_set_exception_service_fn *)GetProcAddress(dll, FLA_SET_EXCEPTION_SERVICE_STR);
// fla_set_exc must not be NULL — validate and skip the DLL if it is
flp_init_exception_service(fla_set_exc);
```

### Application side

**Headers to include:**

```c
#include <faultline/fla_exception_service.h> // FL_TRY / FL_CATCH macros, fla_set_exception_service,
                                             //   g_fla_exception_service
// Pulled in automatically by the above:
//   <faultline/fl_exception_service.h>
//   <faultline/fl_exception_types.h>
```

For assertion macros (`FL_ASSERT_TRUE`, `FL_ASSERT_NOT_NULL`, etc.):

```c
#include <faultline/fl_exception_service_assert.h>
```

**Source files to compile and link:**

```
src/fla_exception_service.c   — g_fla_exception_service global + fla_set_exception_service export
src/fl_exception_service.c    — shared reason-string constants (each DLL needs its own copy)
```

**Export requirement:** The linker must export `fla_set_exception_service`. Compile with `/DDLL_BUILD` (which triggers `FL_DECL_SPEC` → `__declspec(dllexport)`).

## 2. Logging Service

The log service is **optional**. If a DLL does not export `fla_set_log_service` the platform simply skips the injection and the DLL's `default_write` stub will abort if it is ever called — so in practice, application code that calls `LOG_*` macros should always include the log service source files.

### Platform side

**Headers to include:**

```c
#include <flp_log_service.h>                 // LOG_* macros, flp_log_init/cleanup/set_level,
                                             //   flp_write_log, flp_init_log_service
// Pulled in automatically by the above:
//   <faultline/fl_log_types.h>              // FLLogService, FLLogLevel, fl_write_log_fn,
//                                           //   fla_set_log_service_fn, FLA_SET_LOG_SERVICE_STR
```

**Source files to compile and link:**

```
src/flp_log_service.c         — logger state, mutex, flp_write_log, lifecycle, flp_init_log_service
src/fl_threads.c              — C11-style mutex used by flp_log_service.c
```

**Lifecycle:**

```c
flp_log_init();               // call once at driver startup; outputs to stdout at LOG_INFO level
// or
flp_log_init_custom(LOG_DEBUG, "run.log");   // custom level and output file path

// ... run tests ...

flp_log_cleanup();            // call once at driver shutdown
```

Optional tuning after init:

```c
flp_log_set_level(LOG_DEBUG);
flp_log_set_output(some_file_ptr);      // caller retains ownership; logger will not close it
flp_log_set_output_path("driver.log");  // logger opens and owns the file
```

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_log_types.h>          // fla_set_log_service_fn, FLA_SET_LOG_SERVICE_STR

fla_set_log_service_fn *fla_set_log =
    (fla_set_log_service_fn *)GetProcAddress(dll, FLA_SET_LOG_SERVICE_STR);
if (fla_set_log != NULL) {                   // log service is optional
    flp_init_log_service(fla_set_log);
}
```

### Application side

**Headers to include:**

```c
#include <faultline/fla_log_service.h>       // LOG_* macros, fla_set_log_service,
                                             //   g_fla_log_service
// Pulled in automatically by the above:
//   <faultline/fl_log_types.h>
```

Alternatively, include the unified selector header, which picks the right side automatically:

```c
// Platform target built with /DFL_BUILD_DRIVER selects the flp_ side;
// application code (no such flag) selects the fla_ side.
#include <faultline/fl_log.h>
```

**Source files to compile and link:**

```
src/fla_log_service.c         — g_fla_log_service global + default stubs + fla_set_log_service export
```

**Export requirement:** Compile with `/DDLL_BUILD`.

## 3. Memory Service

The memory service is **optional**. Application code that uses standard `malloc`/`free` should include `fla_memory_service.h` (or `fl_memory.h`), which redefines those symbols as macros that route through `g_fla_memory_service`. This gives the platform full visibility into allocations for fault injection and leak detection.

> **Important:** Include `<stdlib.h>` before `fla_memory_service.h` (or `fl_memory.h`). The
> latter redefines `malloc`, `free`, `calloc`, `realloc`, and `aligned_alloc` as macros; if
> `<stdlib.h>` is processed afterwards its declarations may conflict. `fla_memory_service.h`
> already enforces this ordering with an `IWYU pragma: keep` comment on its own `<stdlib.h>`
> include.

### Platform side

**Headers to include:**

```c
#include <flp_memory_service.h>              // flp_malloc/free/calloc/..., flp_init_memory_service,
                                             //   flp_init_fault_memory_service
#include <faultline/flp_memory_context.h>    // FLMemoryContext, flp_init_memory_context
// Pulled in automatically by flp_memory_service.h:
//   <faultline/fl_memory_service.h>         // FLMemoryService, FLMemoryContext (forward decl),
//                                           //   fla_set_memory_service_fn,
//                                           //   FLA_SET_MEMORY_SERVICE_STR
```

For platform code that uses `FL_MALLOC` / `FL_FREE` macros (platform target built with
`/DFL_BUILD_DRIVER`):

```c
#include <faultline/fl_memory.h>             // routes FL_MALLOC etc. to flp_malloc
```

**Source files to compile and link:**

```
src/flp_memory_service.c        — g_memory_service, all six allocator implementations,
                                  flp_init_memory_service, flp_init_memory_context
src/flp_fault_memory_service.c  — fault-injecting allocator backend, flp_init_fault_memory_service,
                                  flp_init_fault_memory_context
```

The memory service depends on the arena allocator and fault injector. Those bring in additional source files; see the `fl_memory.lib` and `faultline.lib` build scripts for the full list.

**Initialization:**

```c
// Arena-only backend
#include <flp_memory_service.h>             // flp_init_memory_service
#include <faultline/flp_memory_context.h>   // FLMemoryContext, flp_init_memory_context

FLMemoryContext flmctx;
flp_init_memory_context(&flmctx, arena);    // 2 args: context + arena
// arena must outlive all DLL test runs
```

For the fault-injecting backend, use `FLFaultMemoryContext` and
`flp_init_fault_memory_context(&flmctx, arena, fi)` from
`<faultline/flp_fault_memory_context.h>`, where `fi = fault_injector_create(arena)` (from
`<faultline/fault_injector.h>`), then inject with `flp_init_fault_memory_service` instead of
`flp_init_memory_service`. Both context headers are public (`include/faultline/`).

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_memory_service.h>     // fla_set_memory_service_fn, FLA_SET_MEMORY_SERVICE_STR

fla_set_memory_service_fn *fla_set_mem =
    (fla_set_memory_service_fn *)GetProcAddress(dll, FLA_SET_MEMORY_SERVICE_STR);
if (fla_set_mem != NULL) {                   // memory service is optional
    flp_init_memory_service(fla_set_mem, &flmctx);
}
```

### Application side

**Headers to include (choose one):**

```c
// Option A — explicit application-side header
#include <faultline/fla_memory_service.h>    // redefines malloc/free/calloc/realloc/aligned_alloc,
                                             //   g_fla_memory_service, fla_set_memory_service

// Option B — unified selector header (recommended when the same source tree builds both sides)
#include <faultline/fl_memory.h>             // picks fla_memory_service.h when FL_BUILD_DRIVER
                                             //   is not defined
```

Either header automatically pulls in:

```c
//   <faultline/fl_memory_service.h>         // FLMemoryService, fla_set_memory_service_fn
```

**Source files to compile and link:**

```
src/fla_memory_service.c      — g_fla_memory_service global + default stubs + fla_set_memory_service export
```

**Export requirement:** Compile with `/DDLL_BUILD`.

### Platform implementation variants

The `FLMemoryService` struct is the only contract between the platform and the application.
Because the application holds nothing but six function pointers, the platform implementation
behind those pointers can be swapped freely — the same application DLL can be loaded by
different drivers without recompilation.

Two natural variants exist:

| Variant | Arena | Fault injector | Suitable for |
|---------|-------|----------------|-------------|
| Arena-only | yes | no | Lightweight drivers that need memory management without fault injection |
| Arena + fault injection | yes | yes | `faultline` and other drivers that inject allocation failures to test error-handling paths |

Both variants populate the same `FLMemoryService` struct and inject it through the same
`fla_set_memory_service` entry point. From the application's point of view the two are
indistinguishable — `malloc`, `free`, etc. resolve through `g_fla_memory_service` regardless
of which driver is running.

**Build-time contract:** This only works if the application was compiled with
`fla_memory_service.h` (or `fl_memory.h`) included, so that its `malloc`/`free`/etc. calls
are already routed through the service macros. An application that calls the system `malloc`
directly bypasses the service entirely and is invisible to both platform variants.

## Quick-Reference: Compile and Link Summary

### Platform executable

| Service   | Headers                   | Source files                                        | Required?   |
| --------- | ------------------------- | --------------------------------------------------- | ----------- |
| Exception | `flp_exception_service.h` | `flp_exception_service.c`, `fl_exception_service.c` | Optional    |
| Log       | `flp_log_service.h`       | `flp_log_service.c`, `fl_threads.c`                 | Recommended |
| Memory    | `flp_memory_service.h` + a `faultline/` context header | `flp_memory_service.c`, `flp_fault_memory_service.c` (+ arena + fault injector)   | Optional    |

### Application DLL

| Service   | Headers                             | Source files                                        | Exports                     |
| --------- | ----------------------------------- | --------------------------------------------------- | --------------------------- |
| Exception | `faultline/fla_exception_service.h` | `fla_exception_service.c`, `fl_exception_service.c` | `fla_set_exception_service` |
| Log       | `faultline/fla_log_service.h`       | `fla_log_service.c`                                 | `fla_set_log_service`       |
| Memory    | `faultline/fla_memory_service.h`    | `fla_memory_service.c`                              | `fla_set_memory_service`    |

Compile all application DLLs with `/DDLL_BUILD` so `FL_DECL_SPEC` expands to
`__declspec(dllexport)` for the setter functions.

## Injection Order

The three services are not fully independent. On the platform side, `flp_memory_service.c`
uses `FL_TRY`/`FL_CATCH` blocks and `FL_ASSERT_*` macros throughout its allocator
implementations, and calls `LOG_DEBUG` inside `flp_free` and `flp_free_pointer`. This means
the platform memory service has hard dependencies on both the exception service and the log
service at runtime. If the memory service is injected before either of those is set up, any
allocation or free call that hits a fault or an assertion will invoke an uninitialized function
pointer.

The exception and log services have no dependencies on each other or on the memory service —
they can be initialized in either relative order.

Call the injectors in this order after each `LoadLibrary`, before running any test code:

1. Log service (optional — must precede memory service; `flp_free` and `flp_free_pointer` call
   `LOG_DEBUG` to record free operations)
2. Exception service (optional in general, but required if the memory service is also being
   injected — every allocator function in `flp_memory_service.c` wraps its body in
   `FL_TRY`/`FL_CATCH`, and the init functions use `FL_ASSERT_NOT_NULL`)
3. Memory service (optional — safe to inject only after both of the above are in place)

```c
// Log (optional)
fla_set_log_service_fn *fla_set_log =
    (fla_set_log_service_fn *)GetProcAddress(dll, FLA_SET_LOG_SERVICE_STR);
if (fla_set_log != NULL) {
    flp_init_log_service(fla_set_log);
}

// Exception (optional; required when also injecting the memory service)
fla_set_exception_service_fn *fla_set_exc =
    (fla_set_exception_service_fn *)GetProcAddress(dll, FLA_SET_EXCEPTION_SERVICE_STR);
if (fla_set_exc != NULL) {
    flp_init_exception_service(fla_set_exc);
}

// Memory (optional) — arena-only shown; fault programs instead call
//   flp_init_fault_memory_service(fla_set_mem, &flmctx) with an FLFaultMemoryContext.
fla_set_memory_service_fn *fla_set_mem =
    (fla_set_memory_service_fn *)GetProcAddress(dll, FLA_SET_MEMORY_SERVICE_STR);
if (fla_set_mem != NULL) {
    flp_init_memory_service(fla_set_mem, &flmctx);
}
```

## A Note on Design Patterns
The service pattern is a composition of several well-known patterns. No single one covers it completely, but the pieces map cleanly.

### GoF Patterns Present
**Strategy**. The FLFooService struct of function pointers is a Strategy. It defines a family of interchangeable behaviors (implementations) behind a stable interface (the struct layout). The application code calls through the pointers without knowing which implementation is behind them. In C++ this would be a pure abstract base class; in C, a vtable-style struct achieves the same thing.

**Bridge**. The three-layer split (`fl_`/`flp_`/`fla_`) maps closely to Bridge, which separates an abstraction from its implementation so the two can vary independently. The `fl_` layer is the abstraction (shared contract), `flp_` is the concrete implementation, and the function pointers are the bridge between them. The application never takes a compile-time dependency on `flp_`.

### Post-GoF Patterns Present
**Dependency Injection / Inversion of Control (Fowler, 2004)**. This is the most precise name for the overall injection sequence. Specifically, it is setter injection: the platform resolves `fla_set_*_service()` and calls it to populate `g_fla_*_service` inside the DLL. The application declares what it needs (the global + setter); the platform satisfies that dependency at runtime. The control of wiring lives entirely outside the component being wired.

**Plugin (Fowler, Patterns of Enterprise Application Architecture)**. The use of `LoadLibrary` + `GetProcAddress` on a well-known symbol string (`FLA_SET_EXCEPTION_SERVICE_STR`) is the Plugin pattern. The platform and the DLL share only a name-based contract; no link-time coupling is required. This is the mechanism that makes the DI possible across a process/DLL boundary.

**Null Object**. The default stubs in the `fla_` layer are a Null Object: safe no-op implementations that prevent crashes if injection hasn't happened yet, without forcing callers to null-check every call site.

### How They Compose

```
Plugin (LoadLibrary + GetProcAddress)
  └─ delivers Dependency Injection (setter injection via fla_set_*)
        └─ injects a Bridge (fl_ abstraction / flp_ implementation)
              └─ whose interface is structured as a Strategy (function-pointer vtable)
                    └─ with a Null Object as the safe default
```

The GoF book doesn't have a name for the full assembly, because it predates the DI literature and doesn't address dynamic loading as a composition mechanism. Martin Fowler's writing is the better reference for the injection and plugin layers specifically.
