# Service Integration Guide
This guide covers how to wire up FaultLine's runtime services — **exception handling**, **logging**, **memory**, **timing**, **file I/O**, and **stream I/O** — when building a host that loads and drives service consumers.

> To bring the service source into another repository in the first place — and
> keep it updated as packages change — see the
> [Service Distribution Guide](service-distribution.md). It opens with a
> step-by-step quickstart covering the whole export path: produce a package,
> import it into the consumer, and wire it into the consumer's build.

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
3. Platform resolves the DLL's setter with `GetProcAddress` using the well-known symbol name (e.g., `FLA_SET_LOG_SERVICE_STR`).
4. Platform calls `flp_init_*_service(fla_set)`, which fills `g_fla_*_service` inside the DLL.
5. Application code now calls through `g_fla_*_service` function pointers transparently.

## 1. Exception Handling

Exceptions are **not a service**: there is no vtable, no setter and nothing to inject.
Every binary compiles `fl_exception.c` exactly once and calls `fl_push`/`fl_pop`/
`fl_throw` directly through the `FL_TRY` family. A platform binary and a DLL it loads
each get their own `static FL_THREAD_LOCAL` environment stack, so a throw is caught in
the image that raised it and nothing unwinds across the module boundary.

**Headers to include:**

```c
#include <faultline/fl_try.h>               // FL_TRY / FL_CATCH* / FL_THROW* / FL_END_TRY
// Pulled in automatically by the above:
//   <faultline/fl_exception.h>             // fl_push/fl_pop/fl_throw, fl_expected_failure,
//                                          //   fl_invalid_value, ...
//   <faultline/fl_exception_types.h>       // FLExceptionEnvironment, FLExceptionState, ...
```

For assertion macros (`FL_ASSERT_TRUE`, `FL_ASSERT_NOT_NULL`, etc.):

```c
#include <faultline/fl_exception_assert.h>
```

**Source files to compile and link:**

```
src/fl_exception.c            — reason constants, fl_push/fl_pop/fl_throw, fl_throw_assertion
```

**Compile flag:** none. `FL_PLATFORM_BUILD` does not select an exception implementation;
it only selects the sides of the real services through the unified selector headers
(`fl_log.h`, `fl_memory.h`, `fl_timer.h`, `fl_file.h`, `fl_stream.h`).

**Initialization:** none. The environment stack initializes lazily. Ensure any code that
can throw runs under an `FL_TRY` in its own image: a throw with no enclosing `FL_TRY`
on the thread reports the throw site to stderr and aborts.

**Export requirement:** none. A suite DLL exports its cases, not a throw hook.

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
// Platform target built with /DFL_PLATFORM_BUILD selects the flp_ side;
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
`/DFL_PLATFORM_BUILD`):

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
#include <faultline/fl_memory.h>             // picks fla_memory_service.h when FL_PLATFORM_BUILD
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

## 4. Timer Service

The timer service is **optional**. It provides monotonic timing: `now()` captures an
opaque `FLTimestamp` sample, and `elapsed_seconds(start, end)` converts two samples to
seconds. Code on either side normally calls it through the `FL_NOW()` / `FL_ELAPSED()`
macros in the `fl_timer.h` selector header; for start/stop/peek semantics, bind an
`FLStopwatch` (`fl_stopwatch.h`) to the active service via `FL_TIMER_SERVICE()`.

### Platform side

**Headers to include:**

```c
#include <flp_timer_service.h>               // flp_timer_now, flp_timer_elapsed_seconds,
                                             //   flp_timer_service, flp_init_timer_service
// Pulled in automatically by the above:
//   <faultline/fl_timer_service.h>          // FLTimestamp, FLTimerService,
//                                           //   fla_set_timer_service_fn,
//                                           //   FLA_SET_TIMER_SERVICE_STR
```

**Source files to compile and link:**

```
src/flp_timer_service.c      — QueryPerformanceCounter backend + flp_init_timer_service
```

**Dependencies:** the provider throws `FL_THROW_DETAILS` if querying the
performance-counter frequency fails, and `flp_init_timer_service` uses
`FL_ASSERT_NOT_NULL`, so `fl_exception.c` must also be compiled in.

**Initialization:** none required. The seconds-per-tick factor is queried lazily on
first use.

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_timer_service.h>      // fla_set_timer_service_fn,
                                             //   FLA_SET_TIMER_SERVICE_STR

fla_set_timer_service_fn *fla_set_timer =
    (fla_set_timer_service_fn *)GetProcAddress(dll, FLA_SET_TIMER_SERVICE_STR);
if (fla_set_timer != NULL) {                 // timer service is optional
    flp_init_timer_service(fla_set_timer);
}
```

### Application side

**Headers to include (choose one):**

```c
// Option A — explicit application-side header
#include <faultline/fla_timer_service.h>     // g_fla_timer_service, fla_set_timer_service

// Option B — unified selector header
#include <faultline/fl_timer.h>              // FL_NOW / FL_ELAPSED / FL_TIMER_SERVICE
```

**Source files to compile and link:**

```
src/fla_timer_service.c      — g_fla_timer_service global + abort stubs + fla_set_timer_service export
```

**Export requirement:** Compile with `/DDLL_BUILD`.

## 5. File Service

The file service is **optional**. It provides positional file I/O over UTF-8 paths:
`read` and `write` each name the byte offset to act on and keep no implicit file
pointer, so a handle is safe to use from several threads. A short transfer count
signals EOF or error, and is where the platform can inject I/O faults. Append-only
writes and console output are provided by a separate stream service (section 6,
below), not this one. Code on either side normally calls it through the
`FL_FILE_OPEN`/`FL_FILE_READ`/`FL_FILE_WRITE`/`FL_FILE_CLOSE` macros in the `fl_file.h`
selector header.

### Platform side

**Headers to include:**

```c
#include <flp_file_service.h>                // flp_file_open/read/write/close,
                                             //   flp_init_file_service
// Pulled in automatically by the above:
//   <faultline/fl_file_service.h>           // FLFileService, fla_set_file_service_fn,
//                                           //   FLA_SET_FILE_SERVICE_STR
//   <faultline/fl_file_types.h>             // FLFile (opaque), FLFileMode
```

**Source files to compile and link:**

```
src/flp_file_service.c       — CreateFileW/ReadFile/WriteFile backend + flp_init_file_service
```

**Dependencies:** the provider allocates through `FL_MALLOC`/`FL_FREE` when converting
extended-length (`\\?\`) paths, so the platform memory service must be initialized
before the provider opens paths longer than `MAX_PATH`; it also uses
`FL_ASSERT_NOT_NULL`, so `fl_exception.c` must be compiled in.

**Initialization:** none required beyond the memory-service setup above.

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_file_service.h>       // fla_set_file_service_fn,
                                             //   FLA_SET_FILE_SERVICE_STR

fla_set_file_service_fn *fla_set_file =
    (fla_set_file_service_fn *)GetProcAddress(dll, FLA_SET_FILE_SERVICE_STR);
if (fla_set_file != NULL) {                  // file service is optional
    flp_init_file_service(fla_set_file);
}
```

### Application side

**Headers to include (choose one):**

```c
// Option A — explicit application-side header
#include <faultline/fla_file_service.h>      // g_fla_file_service, fla_set_file_service

// Option B — unified selector header
#include <faultline/fl_file.h>               // FL_FILE_OPEN / READ / WRITE / CLOSE
```

**Source files to compile and link:**

```
src/fla_file_service.c       — g_fla_file_service global + abort stubs + fla_set_file_service export
```

**Export requirement:** Compile with `/DDLL_BUILD`.

> **Async file I/O:** `fl_async_file_service.h` is a contract sketch only. It has no
> platform provider or consumer accessor yet, and `flp_inject_services()` does not
> inject it.

## 6. Stream Service

The stream service is **optional**. It provides sequential, non-addressable I/O:
append-only writes to a file by UTF-8 path (Windows `FILE_APPEND_DATA` — every
write lands atomically at end of file, so several writers can interleave whole
records) and writes to the process's inherited console streams (stdout/stderr) via
a `console()` accessor. Neither target has a meaningful byte offset, so unlike the
file service's `write`, this contract's `write` takes no offset parameter at all.
`close()` on a `console()`-obtained handle is a documented no-op — stdout/stderr are
process-wide and shared, never this service's to close. It reuses the `FLFile`
opaque type from the file service's `fl_file_types.h` rather than defining its own;
a handle from one service is not interchangeable with a handle from another. Code
on either side normally calls it through the
`FL_STREAM_OPEN`/`FL_STREAM_WRITE`/`FL_STREAM_CLOSE`/`FL_STREAM_CONSOLE` macros in
the `fl_stream.h` selector header.

### Platform side

**Headers to include:**

```c
#include <flp_stream_service.h>              // flp_stream_open/write/close/console,
                                             //   flp_init_stream_service
// Pulled in automatically by the above:
//   <faultline/fl_stream_service.h>         // FLStreamService, FLConsoleStream,
//                                           //   fla_set_stream_service_fn,
//                                           //   FLA_SET_STREAM_SERVICE_STR
//   <faultline/fl_file_types.h>             // FLFile (opaque; reused from the file service)
```

**Source files to compile and link:**

```
src/flp_stream_service.c     — CreateFileW/WriteFile (append) + GetStdHandle backend,
                                flp_init_stream_service
```

**Dependencies:** the provider allocates through `FL_MALLOC`/`FL_FREE` only when
converting extended-length (`\\?\`) paths (the common short-path case allocates
nothing), so the platform memory service must be initialized before the provider
opens paths longer than `MAX_PATH`; it also uses `FL_ASSERT_NOT_NULL`, so
`fl_exception.c` must be compiled in.

**Initialization:** none required beyond the memory-service setup above.

**Injection call (after `LoadLibrary`):**

```c
#include <faultline/fl_stream_service.h>     // fla_set_stream_service_fn,
                                             //   FLA_SET_STREAM_SERVICE_STR

fla_set_stream_service_fn *fla_set_stream =
    (fla_set_stream_service_fn *)GetProcAddress(dll, FLA_SET_STREAM_SERVICE_STR);
if (fla_set_stream != NULL) {                // stream service is optional
    flp_init_stream_service(fla_set_stream);
}
```

### Application side

**Headers to include (choose one):**

```c
// Option A — explicit application-side header
#include <faultline/fla_stream_service.h>    // g_fla_stream_service, fla_set_stream_service

// Option B — unified selector header
#include <faultline/fl_stream.h>             // FL_STREAM_OPEN / WRITE / CLOSE / CONSOLE
```

**Source files to compile and link:**

```
src/fla_stream_service.c     — g_fla_stream_service global + abort stubs + fla_set_stream_service export
```

**Export requirement:** Compile with `/DDLL_BUILD`.

## Quick-Reference: Compile and Link Summary

### Platform executable

| Service   | Headers                   | Source files                                        | Required?   |
| --------- | ------------------------- | --------------------------------------------------- | ----------- |
| Exception | `faultline/fl_try.h`      | `fl_exception.c`                                    | Optional    |
| Log       | `flp_log_service.h`       | `flp_log_service.c`, `fl_threads.c`                 | Recommended |
| Memory    | `flp_memory_service.h` + a `faultline/` context header | `flp_memory_service.c`, `flp_fault_memory_service.c` (+ arena + fault injector)   | Optional    |
| Timer     | `flp_timer_service.h`     | `flp_timer_service.c`                               | Optional    |
| File      | `flp_file_service.h`      | `flp_file_service.c`                                | Optional    |
| Stream    | `flp_stream_service.h`    | `flp_stream_service.c`                              | Optional    |

### Application DLL

| Service   | Headers                             | Source files                                        | Exports                     |
| --------- | ----------------------------------- | --------------------------------------------------- | --------------------------- |
| Exception | `faultline/fl_try.h`                | `fl_exception.c`                                    | none                        |
| Log       | `faultline/fla_log_service.h`       | `fla_log_service.c`                                 | `fla_set_log_service`       |
| Memory    | `faultline/fla_memory_service.h`    | `fla_memory_service.c`                              | `fla_set_memory_service`    |
| Timer     | `faultline/fla_timer_service.h`     | `fla_timer_service.c`                               | `fla_set_timer_service`     |
| File      | `faultline/fla_file_service.h`      | `fla_file_service.c`                                | `fla_set_file_service`      |
| Stream    | `faultline/fla_stream_service.h`    | `fla_stream_service.c`                              | `fla_set_stream_service`    |

Compile all application DLLs with `/DDLL_BUILD` so `FL_DECL_SPEC` expands to
`__declspec(dllexport)` for the setter functions.

## Packaged Injection: `flp_inject_services()`

The per-service `GetProcAddress` + `flp_init_*_service` sequence shown in each section
above is packaged in `src/flp_module_service.c`, which also wraps module loading
(`LoadLibraryA` / `GetProcAddress` / `FreeLibrary`) behind the `FLPlatformAPI` module
primitives declared in `platform_api.h`. A host that uses `FLPlatformAPI` gets loading
and injection as two calls:

```c
#include <flp_module_service.h>      // flp_module_service_init, flp_load_module,
                                     //   flp_inject_services

flp_module_service_init(&fault_ctx); // once at host setup: bind the fault-injecting
                                     //   memory context (FLFaultMemoryContext)

FLModule *suite = flp_load_module(path);
if (suite == NULL || !flp_inject_services(suite)) {
    // load failed, or the suite lacks a required service — skip it
}
```

`flp_inject_services()` resolves every `fla_set_*_service` symbol the suite exports and
injects the matching platform service, in the order log → memory → timer → file →
stream. Exceptions are not in that list: they are not a service, and the suite
compiles its own `fl_exception.c`. It embodies one policy choice a hand-rolled host is
free to make differently:

- **Every service is optional** — a missing setter is simply skipped.
- **The memory binding is always the fault-injecting variant**
  (`flp_init_fault_memory_service` with the context bound by
  `flp_module_service_init()`). The plain arena-only service is what the host installs
  into its own core; the fault-injecting one is injected only into suites, so the
  framework can never fault-inject its own bookkeeping.

The next section explains the ordering constraints behind that sequence, for hosts that
hand-roll injection instead.

## Injection Order

The services are not fully independent. On the platform side, `flp_memory_service.c`
uses `FL_TRY`/`FL_CATCH` blocks and `FL_ASSERT_*` macros throughout its allocator
implementations, and calls `LOG_DEBUG` inside `flp_free` and `flp_free_pointer`. This means
the platform memory service has a hard dependency on the log service at runtime. If the
memory service is injected before the log service is set up, any allocation or free call
that logs will invoke an uninitialized function pointer. Its use of `FL_TRY`/`FL_ASSERT_*`
needs nothing injected: `fl_exception.c` is linked in, not wired up.

The exception and log services have no dependencies on each other or on the memory service —
they can be initialized in either relative order. The timer, file, and stream services have
no dependencies on the other *injected* services (their injectors assert through the
platform's own statically linked exception machinery), so their position in the order is
unconstrained; injecting them last matches `flp_inject_services()`.

Call the injectors in this order after each `LoadLibrary`, before running any test code:

1. Log service (optional — must precede memory service; `flp_free` and `flp_free_pointer` call
   `LOG_DEBUG` to record free operations)
2. Exception service (optional in general, but required if the memory service is also being
   injected — every allocator function in `flp_memory_service.c` wraps its body in
   `FL_TRY`/`FL_CATCH`, and the init functions use `FL_ASSERT_NOT_NULL`)
3. Memory service (optional — safe to inject only after both of the above are in place)
4. Timer service (optional — no ordering constraints)
5. File service (optional — no per-suite ordering constraints; the platform-side
   memory setup it needs for extended-length paths happens at host startup, not per
   suite)
6. Stream service (optional — same shape as the file service: no per-suite ordering
   constraints, and the memory setup it needs only matters for paths longer than
   `MAX_PATH`)

```c
// Log (optional)
fla_set_log_service_fn *fla_set_log =
    (fla_set_log_service_fn *)GetProcAddress(dll, FLA_SET_LOG_SERVICE_STR);
if (fla_set_log != NULL) {
    flp_init_log_service(fla_set_log);
}

// Memory (optional) — arena-only shown; fault programs instead call
//   flp_init_fault_memory_service(fla_set_mem, &flmctx) with an FLFaultMemoryContext.
fla_set_memory_service_fn *fla_set_mem =
    (fla_set_memory_service_fn *)GetProcAddress(dll, FLA_SET_MEMORY_SERVICE_STR);
if (fla_set_mem != NULL) {
    flp_init_memory_service(fla_set_mem, &flmctx);
}

// Timer (optional)
fla_set_timer_service_fn *fla_set_timer =
    (fla_set_timer_service_fn *)GetProcAddress(dll, FLA_SET_TIMER_SERVICE_STR);
if (fla_set_timer != NULL) {
    flp_init_timer_service(fla_set_timer);
}

// File (optional)
fla_set_file_service_fn *fla_set_file =
    (fla_set_file_service_fn *)GetProcAddress(dll, FLA_SET_FILE_SERVICE_STR);
if (fla_set_file != NULL) {
    flp_init_file_service(fla_set_file);
}

// Stream (optional)
fla_set_stream_service_fn *fla_set_stream =
    (fla_set_stream_service_fn *)GetProcAddress(dll, FLA_SET_STREAM_SERVICE_STR);
if (fla_set_stream != NULL) {
    flp_init_stream_service(fla_set_stream);
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
