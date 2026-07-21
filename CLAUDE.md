# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Common Development Commands

### Building the Project
- **Build all components**: `build\cmd\all.cmd` - Builds the entire project including all libraries and test suites
- **Build specific components**: Use individual build scripts in `build\cmd\` (e.g., `build\cmd\faultline.cmd`, `build\cmd\arena.cmd`)
- **Build with options**:
  - `build\cmd\all.cmd test` - Build and run all unit tests
  - `build\cmd\all.cmd debug` - Build debug version (default)
  - `build\cmd\all.cmd release` - Build optimized release version
  - `build\cmd\all.cmd x64` - Build for 64-bit (default)
  - `build\cmd\all.cmd x86` - Build for 32-bit
  - `build\cmd\all.cmd clean` - Clean current build type
  - `build\cmd\all.cmd cleanall` - Clean all build artifacts

### Running Tests
- **All tests**: `build\cmd\all.cmd test` - Builds and runs all unit test suites
- **Single component test**: `build\cmd\<component>.cmd test` (e.g., `build\cmd\arena.cmd test`)
- **Manual test execution**: After building, use `but.exe` or `faultline.exe` with test DLL files
- **FaultLine tests**: Use `faultline.exe` from the output bin directory for fault injection testing
- **FaultLine with database**: `faultline.exe --db results.sqlite <test_suites>` - Store results in SQLite database

### Code Formatting
- **Clang-format configuration**: `.clang-format` contains the project's formatting rules
- Format follows a custom style with 4-space indentation, 89-column limit, and specific alignment rules

### Documentation Generation
- **Generate docs**: `doxygen Doxyfile` - Generates HTML documentation in `docs/html/`
- Documentation includes all C source files, headers, and README

### Performance Analysis
- **Manual analysis**: See `PERFORMANCE_ANALYSIS_MANUAL.md` for manual performance testing procedures
- **Performance reports**: Generated in `PERFORMANCE_REPORT.md`

## Architecture Overview

### Two architectural axes

Two independent distinctions run through the service code. They were historically
both called "platform / application," which conflated them; keep them separate.

**Axis 1 — Portability (this is the one reuse cares about).**
- **core**     — portable, OS-independent code: the service *contracts* and their
                 *portable implementations*. This is what travels to another project.
- **platform** — OS-specific code: the concrete `flp_*` implementations,
                 `region_windows.c`, the Win32 host. Reprovided by each host; does
                 not travel.

`core / platform` names the portability axis directly (the pairing engines,
runtimes, and embedded SDKs use). Prefer it over "application," which smuggles in
a host/guest hierarchy that is a *different* axis.

**Axis 2 — Service provider / consumer (the injection mechanism).**
- A **provider** (`flp_`) supplies a concrete service implementation.
- A **consumer** (`fla_`) receives an injected service through a `g_fla_*` global.
  The consumer may be the core itself (`faultline_app_main` installs services into
  its own globals) *or* a loaded suite DLL. "Consumer," not "application" — the core
  consumes its own services too.

Prefix map: `fl_` = shared contract (core, portable) · `flp_` = platform provider ·
`fla_` = consumer accessor.

**What travels for reuse:** a service's `fl_*_service.h` contract **plus** its
portable core implementation (e.g. `fl_stopwatch.h`, `arena.c`, the `fl_threads`
polyfill). The `flp_*` platform implementation is reprovided per host. The Faultline
glue that assembles services — `platform_api.h`, `FLPlatformAPI`, the Win32 host —
is inherently Faultline-specific and stays Faultline-named.

**Vocabulary rule — rename across the line, not within it.** In a contract header
or a portable core implementation, use neutral actor words (**caller**, **consumer**,
**the core**) and keep the **axis** words (**core**, **platform**). Do **not** leak
Faultline-role words (**driver**, **suite**, **test**) into them. Everything that
assembles services into Faultline is allowed to use those role words freely
(`faultline_driver.c`'s driver, the suites it loads, etc.).

Exception: where a contract header genuinely encodes domain semantics — e.g.
`fl_exception_service.h`, whose `fl_expected_failure` is defined in terms of test
cases and the test driver — keep the test vocabulary honest rather than launder it.
That service is the least drop-in by nature, and that's fine.

### Modular Library Architecture (v0.2.0)

FaultLine is organized into **6 modular component libraries** that can be used independently:

#### fl_log.lib - Logging System (Platform provider depends on the file service)
- Public API: `include/faultline/fl_log_service.h` (contract), `include/faultline/fl_log.h` (unified `LOG_*` selector)
- Implementation: `src/flp_log_service.c` (platform provider), `src/fla_log_service.c` (consumer accessor)
- Features: Service-based logging with configurable levels and backends, selected per translation unit by `FL_PLATFORM_BUILD`
- Dependency: the platform provider opens path-based log output through the file service (`flp_file_open`/`flp_file_write`/`flp_file_close`, `FL_FILE_APPEND` mode) for its atomic-append and fault-injection-testability guarantees; stdout output stays raw stdio, since the file service has no notion of it. The consumer accessor has no dependencies.
- Build script: `build/cmd/log_service.cmd`

#### fl_math.lib - Math Utilities (No Dependencies)
- Public API: `include/math/math.h`, `include/math/math_windows.h`
- Implementation: `src/math.c`
- Features: Platform-safe mathematical operations and Windows-specific utilities
- Build script: `build/cmd/fl_math_lib.cmd`

#### fl_memory.lib - Memory Management (Depends on: fl_log)
- Public API:
  - `include/memory/arena.h` - Arena allocator with opaque type
  - `include/memory/buffer.h` - Dynamic buffers
- Implementation:
  - `src/arena.c`, `src/arena_dbg.c`, `src/arena_malloc.c`, `src/arena_malloc_throw.c`
  - `src/chunk.c` - Memory chunk management (private)
  - `src/region.c`, `src/region_node.c` - Large region management (private)
  - `src/digital_search_tree.c` - Internal data structure (private)
  - `src/buffer.c`, `src/sysalloc_windows.c`
- Private headers: `src/arena_internal.h`, `src/chunk.h`, `src/region.h`, etc.
- Features: Arena allocator, buffers, region management, mid-level inspection APIs
- Build script: `build/cmd/fl_memory_lib.cmd`

#### fl_collections.lib - Data Structures (Depends on: fl_memory, fl_log)
- Public API:
  - `include/collections/set.h` - Hash set
  - `include/collections/dlist.h` - Doubly-linked list macros
- Implementation:
  - `src/set.c`
  - `src/FNV64.c` - FNV hash functions (private)
  - `src/dlist.c` (if separate file, else header-only)
- Private headers: `src/FNV64.h`, `src/FNVconfig.h`, etc.
- Features: Hash sets, intrusive lists, FNV-1a hashing
- Build script: `build/cmd/fl_collections_lib.cmd`

#### fl_timer.lib - Performance Timing (Depends on: fl_log)
- Public API: `include/timer/timer.h`
- Implementation:
  - `src/time_timer.c` - Standard time-based timer
  - `src/win_timer.c` - Windows QueryPerformanceCounter timer
  - `src/tick_timer.c` - CPU tick timer (optional)
- Private headers: `src/time_timer.h`, `src/win_timer.h`, `src/tick_timer.h`
- Features: Three timer implementations for different use cases
- Build script: `build/cmd/fl_timer_lib.cmd`

#### faultline.lib - Fault Injection Framework (Depends on: all above)
- Public API:
  - `include/faultline/faultline.h` - Main FaultLine API
  - `include/faultline/faultline_context.h` - Test execution context
  - `include/faultline/faultline_types.h` - Core type definitions
  - `include/faultline/faultline_result_codes.h` - Result codes
  - `include/faultline/fault_malloc.h` - Memory fault injection
  - `include/faultline/fault_injector.h` - General fault injection
- Implementation:
  - `src/faultline_driver.c` - Test driver logic
  - `src/faultline_context.c` - Context management
  - `src/faultline_sqlite.c` - SQLite persistence
  - `src/fault_injector.c`, `src/fault_malloc.c` - Fault injection
  - `third_party/sqlite/sqlite3.c` - Embedded SQLite
- Command-line interface: `cmd/faultline/main_windows.c`
- Features: Complete fault injection testing with database persistence
- Build script: `build/cmd/faultline_lib.cmd`

#### Library Dependency Graph
```
faultline.lib
  ├─> fl_collections.lib
  │     ├─> fl_memory.lib
  │     │     └─> fl_log.lib
  │     └─> fl_log.lib
  ├─> fl_memory.lib
  ├─> fl_timer.lib
  │     └─> fl_log.lib
  └─> fl_log.lib

fl_math.lib (no dependencies)
```

### Core Components

**BUT (Basic Unit Test)**: The underlying unit testing framework (built from source).
- Build script: `build/cmd/but.cmd` - Builds `but.exe` test driver
- Headers: `include/but/but.h`, `include/but/but_macros.h`, `include/but/but_assert.h`
- Provides test case/suite structures and test execution

**Exception Handling Service**: Custom C exception handling using setjmp/longjmp with service-based architecture.
- Shared API: `include/faultline/fl_exception_service.h` - Service interface (`FLExceptionService` struct with push/pop/throw function pointers)
- Types: `include/faultline/fl_exception_types.h` - `FLExceptionEnvironment`, `FLExceptionReason`, `FLExceptionState`
- Unified macros: `include/faultline/fl_try.h` - the **single definition site** for `FL_TRY`/`FL_CATCH*`/`FL_THROW*`/`FL_END_TRY`; selects the platform provider or consumer accessor by `FL_PLATFORM_BUILD`, then defines the family once over the `FL_EXC_PUSH/POP/THROW` hooks
- Assertions: `include/faultline/fl_exception_service_assert.h` - `FL_ASSERT_*` macros that throw exceptions (includes `fl_try.h`)
- Platform provider: `include/flp_exception_service.h` (declares `flp_push`/`flp_pop`/`flp_throw`, `flp_init_exception_service`), `src/flp_exception_service.c` (owns TLS exception stack, implements push/pop/throw)
- Consumer accessor: `include/faultline/fla_exception_service.h` (declares `g_fla_exception_service` and `fla_set_exception_service`), `src/fla_exception_service.c` (TLS service with default-abort stubs)
- Service injection: Driver calls `flp_init_exception_service()` to fill the struct, then after `LoadLibrary()` resolves `fla_set_exception_service` via `GetProcAddress` and calls it to inject the service into the DLL

**Log Service**: Logging service following the same platform-provider/consumer pattern as exceptions.
- Shared API: `include/faultline/fl_log_service.h` - Service interface (`FLLogService` struct with write function pointer)
- Unified macros: `include/faultline/fl_log.h` - the **single definition site** for `LOG_*`; selects `flp_write_log` or `g_fla_log_service.write` by `FL_PLATFORM_BUILD`
- Platform provider: `include/flp_log_service.h`, `src/flp_log_service.c` · Consumer accessor: `include/faultline/fla_log_service.h`, `src/fla_log_service.c`
- `flp_log_set_output_path` opens its target through the file service (`flp_file_open` in `FL_FILE_APPEND` mode) instead of `fopen_s` directly, so every write is one atomic append (`flp_file_write`) rather than several streamed `fprintf` calls, and the write path is exercisable by FaultLine's own fault injection. `flp_log_set_output` (stdout / a caller-supplied `FILE*`) stays raw stdio — the file service has no notion of it.

**Public vs Private Headers**:
- **Public headers**: In `include/` subdirectories (log/, math/, memory/, collections/, timer/, faultline/)
  - Exported in SDK package
  - Opaque types where appropriate (e.g., Arena)
  - Versioned with FL_*_VERSION_* macros
- **Private headers**: In `src/` directory
  - Implementation details only
  - Not exported in SDK
  - Full structure definitions

### Build System Structure

The build system uses Windows batch files with Visual Studio compiler:
- **Build scripts location**: `build/cmd/`
- **Output structure**: `target/<vs_version>/<platform>/<build_type>/`
  - Example: `target/vs2022/x64/debug/bin/`
- **Compiler configuration**: `build/cmd/config.cmd` - MSVC flags and settings
- **Environment setup**: `build/cmd/setup.cmd` - Build environment initialization

### Test Organization

Tests are organized as shared libraries (DLLs) loaded by test drivers:
- **BUT tests**: Can be loaded by `but.exe` test driver
- **FaultLine tests**: Loaded by `faultline.exe` with fault injection capabilities
- Test files follow pattern `*_test.c` and `*_test.h`
- Each component has its own test suite DLL
- Test data structures defined in `*_test_data.h` files
- **Test output**: Located in `test/` directory with compiled DLLs and executables
- **Database integration**: FaultLine can store test results in SQLite databases
- **Test compilation**: All test DLLs must be compiled with `/DDLL_BUILD`

### Platform Considerations

- **Primary platform**: Windows with MSVC compiler (VS2017/2019/2022)
- **C Standard**: C17 with experimental C11 atomics support
- **Threading**: Uses Windows-specific threading and atomic operations
- **Architecture**: Supports both x86 and x64 builds

### Key Conventions

- **Naming**: Use snake_case for functions and variables, UPPER_CASE for macros
- **Error Handling**: Uses custom exception system, not errno-based errors
- **Memory**: Manual memory management with arena-based allocation
- **Headers**: Include guards use `FILENAME_H_` pattern
- **Build**: Debug builds include assertions and debugging info, release builds are optimized
- **Code formatting**: Use `.clang-format` configuration for consistent styling
- **Documentation**: Follow Doxygen commenting conventions

### Commit Message Convention

Use **`scope: description`** (the Linux/Git/Go style), not Conventional Commits (`type:`).
The scope — the affected subsystem — is the most useful key for scanning history, so it
leads the subject line. The verb in the description already conveys whether a change is a
fix, feature, or refactor, so a separate `type:` prefix is redundant.

**Format**: `scope: imperative description` (subject ≤ ~72 chars, lowercase scope, no trailing period).

**Scopes** are the component libraries and top-level areas:
- Libraries: `fl_log`, `fl_math`, `fl_memory`, `fl_collections`, `fl_timer`, `faultline`
- Other areas: `build`, `cmd`, `but`, `test`, `docs`, `third_party`
- For a specific file/tool, a finer scope is fine (e.g., `fl_import:`, `service_demo:`).

**Examples**:
```
faultline: emit per-test <testcase> elements in JUnit output
build: clean and build service_demo in one invocation
fl_import: resolve relative paths against PowerShell's location
fl_memory: fix region coalescing off-by-one
```

**Breaking API changes**: keep the `scope: description` subject and add a `BREAKING CHANGE:`
footer describing the break. This preserves the one machine-readable signal worth keeping
without per-commit type ceremony.

Do not rewrite existing history to this style — apply it to new commits only.

### File Naming Convention

Files use a prefix system marking which side of the service boundary a file belongs to.
This maps to the **provider/consumer** axis described under **Two architectural axes**
above (which also covers the separate **core/platform** portability axis):

| Prefix | Meaning | Examples |
|--------|---------|----------|
| `fl_` | Shared contract / portable core (used by both sides) | `fl_macros.h`, `fl_log_service.h`, `fl_try.h` |
| `flp_` | Platform provider — concrete service implementations | `flp_log_service.c`, `flp_exception_service.c` |
| `flp_win32_` | Platform provider (Windows-specific) | `flp_win32_thread.c` |
| `flp_linux_` | Platform provider (Linux-specific) | `flp_linux_thread.c` |
| `fla_` | Consumer accessor — receives injected services via `g_fla_*` | `fla_log_service.h`, `fla_exception_service.c` |

- **platform** (`flp_`) = OS-specific provider of concrete service implementations
- **consumer** (`fla_`) = receives services injected into its `g_fla_*` globals (the core itself, or a loaded suite DLL)
- A translation unit selects which side it compiles against via `FL_PLATFORM_BUILD` (defined → platform provider; undefined → consumer); the unified selector headers (`fl_try.h`, `fl_log.h`, `fl_memory.h`) act on it
- The third character (`p` or `a`) immediately identifies which side a file belongs to
- OS-specific prefixes group related files together in directory listings

**Macro prefixes** follow the same convention:
- `FL_*` - Shared macros (e.g., `FL_ARRAY_COUNT`, `FL_TRY`, `LOG_*`)
- `FLP_*` - Platform-provider macros (e.g., `FLP_INIT_EXCEPTION_SERVICE_FN`)
- `FLA_*` - Consumer-accessor macros (e.g., `FLA_SET_LOG_SERVICE_FN`)

## Additional Project Structure

### Special Directories

- **cmd/**: Command-line executables
  - `faultline/` - FaultLine fault injection test driver
  - `log_example/` - Logging system example
- **metrics/**: Performance timing data (.ctm files)
- **morgue/**: Deprecated/unused code (historical reference)
- **test/**: Compiled test outputs (executables, DLLs, logs, databases)
- **third_party/**: External dependencies
  - `sqlite/` - SQLite database library integration
- **ctime/**: Build time tracking utilities

### FaultLine Database Features

**Command-line Database Options**:
- `--db PATH` or `--database PATH` - Use specified database file
- `--no-db` - Disable database storage
- Default: `./faultline_results.sqlite` if no database option specified

**Query Commands**:
- `--show-runs` - Display recent test runs with summary statistics
- `--show-failures` - Show test failures with filtering options
- `--show-run ID` - Detailed results for specific run
- `--show-suite NAME` - Suite summary statistics
- `--limit N` - Control result count (default: show all)
- `--log-level LEVEL` - Set logging level (error|warn|info|debug)

**Database Schema**:
- `test_suites` - Test suite metadata
- `test_runs` - Individual test execution runs
- `test_case_executions` - Results for each test case
- `fault_injections` - Fault injection attempt details
- `raw_faults` - Detailed fault information when available

### Performance Analysis Tools

- **Manual procedures**: `PERFORMANCE_ANALYSIS_MANUAL.md` - Step-by-step testing guide
- **Metrics collection**: Automatic timing data collection in `metrics/` directory
- **Reporting**: Comprehensive performance reports with statistical analysis

## External Dependencies

### SQLite

- **Location**: `third_party/sqlite/`
- **Version**: Embedded SQLite amalgamation
- **Usage**: Database persistence for test results in FaultLine

## Environment Notes

- You are running in a bash shell. You can't run .cmd scripts in a bash shell.
- Faultline is a git repo (it migrated from Fossil SCM in July 2026), so use git commands.

## C Coding Guidelines
### Braces
Always wrap if/else, for, while, and do-while blocks in braces, even for a single-line body.

### Function return points
Prefer a single return point at the end of a function. Two exceptions are permitted:

1. Guard clauses — early returns at the top of a function for precondition checks are allowed. They reduce nesting and make preconditions visible at a glance.
2. goto cleanup — use when multiple resources are acquired sequentially and a single consolidated cleanup block would otherwise require either duplicating teardown code at each failure point or nesting one level deeper per acquired resource. Do not use goto cleanup when an if/else or restructured logic handles cleanup without either problem.

```c
/* goto cleanup is justified here: three resources, cleanup in one place */
status = AcquireR1(&r1);
if (!NT_SUCCESS(status)) goto Cleanup;

status = AcquireR2(&r2);
if (!NT_SUCCESS(status)) goto Cleanup;

status = AcquireR3(&r3);
if (!NT_SUCCESS(status)) goto Cleanup;

/* do work */

Cleanup:
    ReleaseR3(r3);
    ReleaseR2(r2);
    ReleaseR1(r1);
    return status;
```

### Switch statements
Default clause: Always provide a default clause. If all enum values are explicitly handled and an unexpected value indicates a programming error, default should log an error and return a failure code. The default clause should always be the last clause in a switch statement.

Braces on case clauses: Omit braces from case clauses unless the clause declares local variables — in that case braces are required to establish scope and prevent initialization from being jumped over.

Fallthrough: Intentional fallthrough must be marked with a /* fallthrough */ comment or __attribute__((fallthrough)). Unintentional fallthrough is a bug.

Break: Each case clause must end with exactly one break, return, or explicit fallthrough marker. Do not use multiple break statements within a single case clause.

Return from a case clause: The general function return rules apply within case clauses. In a dispatch function where each case computes and immediately returns a value, return at the end of a case clause is acceptable in place of break.

### Loop termination
Complexity threshold: A loop condition is too complex if it contains more than one boolean operator (&&, ||) and the operands represent different concerns — i.e., they cannot be read as a single coherent predicate.

Default: Express loop termination in the loop header.

Use break when:

1. The exit condition cannot be evaluated before the loop body executes
2. The loop condition would encode more than one independent reason to stop
3. A flag variable would otherwise be required solely to communicate an exit condition from the body back to the header

continue: Apply the same reasoning as break. Use it when the skip-to-next-iteration condition cannot be expressed cleanly in the loop header or as a structured if/else within the body.

Nested loops: break exits only the innermost loop. To exit an outer loop from an inner one, use goto with a label placed immediately after the outer loop.

### Loop Continuation
continue is useful when expressing the condition structurally would make the code worse. There are two genuinely good cases:

1. Guard clauses inside a loop body

When processing a collection and most items need to be skipped based on a simple predicate, continue at the top of the body mirrors the guard-clause pattern for functions:

```c
for (int i = 0; i < count; i++) {
    if (!is_valid(items[i])) continue;
    if (items[i].type != TARGET_TYPE) continue;

    process(items[i]);
}
```

The alternative is nesting the entire body inside if statements, which pushes the main logic rightward. The continue version keeps the happy path at the left margin.

2. Skipping the remainder of a long body after partial work

When a loop body has distinct phases — validate, prepare, execute — and a failure in an early phase means the later phases must be skipped, continue after the failure avoids either an else block that wraps everything below it or a flag variable:

```c
while (get_next_record(&rec)) {
    if (!parse_record(&rec, &parsed)) {
        log_error("parse failed");
        continue;
    }

    if (!validate(&parsed)) {
        log_error("validation failed");
        continue;
    }

    commit(&parsed);
}
```

When it is not a good idea:

- Mid-body continue with a non-trivial condition — if the continue is buried deep in the body behind several if levels, it is harder to see than a structured else
- continue in a for loop that skips the update expression — for (i = 0; i < n; i++) with a continue still runs i++, which is usually what you want, but if the update is non-trivial (e.g., a pointer advance), skipping it silently is a bug risk worth flagging in review
- continue as a substitute for thinking about loop structure — the same caveat as break; if you find yourself using multiple continue statements for complex interrelated conditions, the loop body probably wants to be a function
