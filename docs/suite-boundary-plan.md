# Suite boundary plan: run C and C++ suites from one driver

Status: proposed, 2026-09-02. Replaces the `cpp-redesign` branch approach.

## Goal

One `faultline.exe`, built as C, loads and exercises test suites built as either C or
C++. No global build switch, no second driver.

## Why the current branch does not get there

`cpp-redesign` welds three things into one flag (`config.cmd:201`):

```
/TP /std:c++20 /DFL_EXC_BACKEND_CXX
```

`cxx` is all-or-nothing across the tree. Driver and suite must agree, and
`fl_abi_check` enforces that agreement (`fl_abi.h:194-197`). The result is two
mutually incompatible worlds, not one driver that reads both. Building C code as C++
was a side effect of that coupling, not the objective.

## The one crossing that causes it

Unwinding crosses the DLL boundary in exactly one place. The driver puts `FL_TRY` on
its side of a call into the suite:

- `faultline_driver.c:52`, `:71`, `:92` — setup, test, cleanup
- `but_driver.c:120`, `:136`, `:154` — same shape

The suite throws, the driver catches. `setjmp` and `longjmp` both run in driver code
and the suite's frames in between are discarded without unwinding. That is why the
backends have to match: it is the driver's jump buffer the suite jumps into.

Fault injection does **not** cross inward. `flp_fault_malloc` catches its own injected
exception and returns `NULL` (`flp_fault_memory_service.c:141-157`). The boundary only
ever needs to carry a result outward.

## The change

Move the catch to the suite side. The boundary carries a value, not an unwind.

```
today                                    proposed
-----                                    --------
driver  FL_TRY {                         driver  fl_run_case(i, &out) ──► DLL
          tc->test(tc) ────────► DLL                                       FL_TRY { ... }
        }                    throws                                        FL_CATCH_ALL
driver  FL_CATCH_ALL ◄──── longjmp       driver  read out    ◄──────────── plain return
          FL_REASON/DETAILS/FILE/LINE            out.status, .failure_type,
                                                 .reason, .details, .file, .line
driver  leaks = allocated - initial      driver  leaks = allocated - initial  (unchanged)
```

The shim is emitted by `FL_GET_TEST_SUITE`, so it is compiled by the suite's own
compiler: `setjmp` in a C suite, `try`/`catch` in a C++ one, from one macro source.
The driver never sets a jump buffer the suite jumps into and never needs a `catch`.

### Why this is safe per DLL

A suite DLL is one unity translation unit. `arena_tests.c` `#include`s `arena.c`,
`buffer.c`, `set.c`, `region.c`, the `fla_*` accessors and the test sources;
`build_test_dll.cmd:86` compiles that single file. One DLL is therefore one dialect
and one backend throughout. A C++ suite's `arena.c` throws the same way its test
bodies do. No mixed-backend hazard inside a module, and no C++ variants of the
static libraries.

### What leak detection does with this

Nothing. It never went through the exception service. The ledger
(`FaultInjector.allocated_resources`) lives in the driver's arena and is written by
`fault_injector_record_allocate` / `record_free`, called from the driver-side
allocators (`flp_fault_memory_service.c:148`, `:103`). The suite reaches it only by
calling through the injected **memory** service, which stays injected — it is the
instrumentation point. Leak and invalid-free arithmetic at `faultline_driver.c:298`,
`:325`, `:364` is untouched.

The exception service is the only service the driver has no reason to own. Nothing
driver-side inspects a throw the suite is going to handle itself.

## Contract

New header, `include/faultline/fl_case_outcome.h`:

```c
typedef enum FLCaseStatus {
    FL_CASE_PASS,
    FL_CASE_EXPECTED_FAILURE,   // fl_expected_failure, matched inside the DLL
    FL_CASE_UNEXPECTED_FAILURE,
} FLCaseStatus;

typedef struct FLCaseOutcome {
    FLCaseStatus  status;
    FLFailureType failure_type;    // FL_SETUP/TEST/CLEANUP_FAILURE, or FL_FAILURE_NONE
    char const   *reason;          // literal in the DLL, valid while it is loaded
    char const   *file;            // same
    int           line;
    double        elapsed_seconds; // the test body only; setup and cleanup fall
                                   // outside the measured region
    char          details[FL_MAX_DETAILS_LENGTH]; // owned copy, not a scratch pointer
} FLCaseOutcome;

// Whether the call reached a case at all -- a different question from how the test
// did, so a separate enum. Folding FL_CASE_NOT_RUN into FLCaseStatus would put a
// value in .status that can never appear there, and would report a driver asking for
// a case that does not exist as a test case that failed. Shaped after FLAbiVerdict,
// string function included.
typedef enum FLRunCaseResult {
    FL_RUN_CASE_OK,           // a case ran; the outcome is filled in
    FL_RUN_CASE_BAD_OUTCOME,  // out is NULL or too small; nothing written
    FL_RUN_CASE_NO_SUCH_CASE, // index past the end; out cleared, no case ran
} FLRunCaseResult;

#define FL_RUN_CASE_FN(name) \
    FLRunCaseResult name(size_t index, FLCaseOutcome *out, size_t out_size)
typedef FL_RUN_CASE_FN(fl_run_case_fn);
extern FL_SPEC_EXPORT fl_run_case_fn fl_run_case;
#define FL_RUN_CASE_STR FL_STR(fl_run_case)
```

`out_size` follows the convention already used by `fla_set_exception_service`
(`fl_exception_service.h:117-118`). The driver passes its own `sizeof`, and the shim
refuses to write a struct larger than that. This is the only mismatch a missing export
cannot catch: a DLL that has `fl_run_case` but was built against a different
`FLCaseOutcome` layout, which is a realistic hazard while the refactor is in progress.

`details` is an inline array, not a pointer. `TestResult` already owns its copy for
exactly this reason (`faultline_test_result.h:36`, and the lifetime comment at `:30`);
putting the array in the outcome means no pointer into the DLL's per-thread scratch
buffer ever crosses the boundary, and the driver's copy discipline stops being
load-bearing. Cost is 512 bytes of driver stack per call, which `TestResult` already
spends.

The `extern` declaration above matters for the same reason it does for `fla_get_abi`
(`fl_abi.h:299-301`): without a prior declaration inside an `extern "C"` block, a C++
suite gives the definition mangled linkage and `GetProcAddress(FL_RUN_CASE_STR)`
stops finding it.

## Work items

### 1. Contract header

Add `fl_case_outcome.h` as above. `FLFailureType` is reused from `fl_types.h`.

### 2. Emit the shim from `FL_GET_TEST_SUITE` (`include/faultline/fl_test.h:167`)

Alongside `fl_get_test_suite` and `fla_get_abi`, emit `fl_run_case`. Body:

- bounds-check `index`, report `FL_CASE_UNEXPECTED_FAILURE` if out of range
- one `FL_TRY` per phase, not one spanning all three. An `fl_expected_failure` thrown
  by setup must leave the body still running, which is what today's three separate
  driver-side `FL_TRY` blocks do; a single spanning `FL_TRY` would skip it.
- the two precedence rules currently at `faultline_driver.c:63` and `:98`: a setup
  failure skips both the body and cleanup; a cleanup failure does not overwrite a
  body failure
- time the body alone with `FL_NOW`/`FL_ELAPSED`, reading `start` before the `FL_TRY`
  so it survives the `longjmp`. This is the region `faultline_driver.c:72`/`:86` times
  today, so what lands in the results database does not change.
- `FL_CATCH(fl_expected_failure)` — inside the DLL the reason pointer and the constant
  are the same object, so this is pointer identity, not `FL_CATCH_STR`. The
  cross-module `strcmp` in `FL_UNEXPECTED_EXCEPTION` goes away from this path.
- copy `FL_DETAILS` into `out->details`

`tc->setup != NULL` / `tc->cleanup != NULL` checks move here from the driver.

### 3. Make the exception service module-local (`src/fla_exception_service.c:25-61`)

Replace the abort stubs with a working implementation: a TLS environment stack plus
`longjmp` under the setjmp backend, `throw FLException` under the C++ backend. This is
what `flp_exception_service.c:38-78` already does; the two files converge.

`fl_throw_assertion` routes to the local implementation.

### 4. Switch the drivers and stop injecting exceptions

**These land together.** Splitting them breaks the tree in one direction: if injection
stops while the driver still holds the catch, the suite's throw longjmps to an empty
suite-side stack and aborts.

- `faultline_driver.c` — `run_timed_test` collapses to one `fl_run_case` call plus
  bookkeeping. No `FL_TRY`, no `setup_failure` reason (`:31`), no `FL_FINALLY` for the
  stopwatch (`:85`) since the call returns normally in every case. The two-`TestResult`
  collapse at `:230-233` becomes a read of `out.failure_type`; the comment at `:226`
  already states those failures are mutually exclusive, so one outcome is sufficient.
  The stopwatch disappears entirely: the shim measures the body and reports
  `out.elapsed_seconds`, so the driver records that instead of timing anything itself.
- `but_driver.c:120-159` — same collapse. Both drivers load the same DLLs, so both use
  the shim.
- `build/dist/selftest/test_framework_host_test.c:110-115` — a third host, same
  collapse. Its file comment at `:13-18` documents the arrangement being removed
  ("the module's throw therefore unwinds to an FL_TRY frame in this file") and has to
  be rewritten to describe the value-returning boundary. The
  `sizeof_exception_env` assertion at `:73` goes with the ABI narrowing in item 5.
- `command_run.c:343-349` — remove the `fla_set_exception_service` lookup. It was the
  only *required* service, so `suite_inject` loses its failure path and returns void.
  Memory, log, timer, file and stream injection are unchanged.

A consequence worth having: the driver stops holding function pointers into the DLL.
`tc->setup`/`tc->test`/`tc->cleanup` become DLL-private. The driver reads `FLTestSuite`
only for `count` and per-case `name`, which it still needs for `display_test_case` and
the log lines. Data across the boundary, no code pointers.

### 5. Narrow the ABI check (`include/faultline/fl_abi.h`)

`backend`, `sizeof_jmp_buf` and `sizeof_exception_env` stop being compared at `:194-197`
— none of them cross the boundary any more. Keep the fields (the struct only grows,
and they stay useful in the diagnostic line); drop `FL_ABI_LAYOUT_MISMATCH` or narrow
it to whatever remains.

What still has to match: CRT, C11 threads types, compatibility version.

**No `FL_COMPATIBILITY_VERSION` bump.** Every suite in this repo is rebuilt with the
driver, and the one external consumer (`worldbuilder`) is rebuilt from regenerated
distribution packages. A stale DLL is detected by its missing `fl_run_case` export,
which names the actual problem; a version number would be a second, vaguer signal for
the same condition. The `out_size` parameter covers the layout case the export check
cannot see.

### 6. Build scripts

`cxx` narrows from a tree-wide switch to a per-suite one.

- Drop it from the driver, host, library and example scripts: `faultline.cmd`,
  `faultline_core.cmd`, `faultline_split.cmd`, `faultline_lib.cmd`,
  `std_faultline.cmd`, `but_driver.cmd`, `index.cmd`, `arena_bench.cmd`,
  `log_example.cmd`, `malloc_cleanup_config.cmd`, `service_demo.cmd`,
  `faultline_analyze.cmd`, `faultline_fixtures.cmd`. These are always C now.
- Keep it in `build_test_dll.cmd:79`, `options.cmd`, and the `CommonCompilerFlagsCXX`
  bundle in `config.cmd` — that is where "build this suite as C++" is a real option.
- `cpp_probe.cmd` keeps its purpose but narrows its unit list to the suite unity TUs.
  Those are the only units that must compile under both dialects.

The per-file C++ compatibility work already on the branch (commit `74dc123`, explicit
casts) is not wasted: library sources are pulled into suite unity TUs, so they still
have to compile as C++. What is discarded is the global backend switch and `/TP` on
the driver.

### 7. Distribution packages

`worldbuilder` is the only consumer outside this repo. It builds against the
`test_framework` package, which today ships three headers and depends on
`exception_service` (`dist/test_framework/manifest.txt`).

- **Done at commit 1**, because `fl_test.h` including `fl_case_outcome.h` broke the
  package's own self-test immediately rather than at this step. `test_framework` gained
  `fl_case_outcome.h` and `fl_types.h` in its `Inc` list, `timer_service` in its
  `Depends`, and a `Version` raise to 0.3.0; `fl_dist_selftest.cmd` imports the new
  dependency and compiles `fla_timer_service.c` into the suite and
  `flp_timer_service.c` into the host. Manifests are generated by the dist scripts,
  not hand-edited.
- `exception_service` ships both `fla_exception_service.c` and
  `flp_exception_service.c`. Once `fla_` is self-sufficient (item 3), a consumer that
  only loads suites needs the `fla_` side alone. Worth revisiting when the packages
  are regenerated, not before.
- Both packages change content, so both need their `Version` raised in
  `packages.psd1` and their manifests regenerated — `test_framework` for the new
  header, `exception_service` for the rewritten `fla_exception_service.c`.

`worldbuilder` rebuilds after this step, not before. Its suites use
`FL_GET_TEST_SUITE`, so they gain `fl_run_case` from the macro and the rebuild is a
recompile against the regenerated packages, with no source changes.

### 8. Acceptance test

Build one existing suite twice — once as C, once with `/TP` and `FL_EXC_BACKEND_CXX` —
and run both through the same unmodified `faultline.exe`. Verify:

- both report the same pass/fail counts
- fault injection discovers the same site count in both
- a deliberate leak in the C++ suite is reported with its allocation file and line
- a C++ test body with a non-trivial destructor has that destructor run on the failure
  path — the case the current design cannot express at all

## Commit sequence

1. `fl_case_outcome.h` + `fl_run_case` emitted from `FL_GET_TEST_SUITE`. Nothing calls
   it. Tree builds, tests pass unchanged. Includes the timer wiring the shim needs:
   `fla_timer_service.c` linked into all 27 suite unity TUs, and the two hosts that
   were not injecting a timer brought level with `faultline.exe` — see **Hosts** below.
2. `fla_exception_service.c` becomes self-sufficient. Injection still overwrites it, so
   behavior is unchanged.
3. All three hosts switch to `fl_run_case`; exception injection removed. Atomic — see
   item 4.
4. ABI check narrowed.
5. Build scripts.
6. C++ suite added and run under the C driver.
7. Distribution packages regenerated; `worldbuilder` rebuilt.

## Hosts

Three hosts load suite DLLs. They were not injecting the same set of services, and the
shim's timing made that difference load-bearing:

| host | injects before | injects now |
|------|----------------|-------------|
| `app/faultline/command_run.c` | log, exception, memory, timer, file, stream | unchanged |
| `app/but/win32_main.c` | log, exception, memory, file, stream | + timer |
| `build/dist/selftest/test_framework_host_test.c` | exception | + timer |

BUT was one service behind, not absent: it already injects the fault-injecting memory
service. Bringing it level is seven lines copied from the five injections beside it.

`timer_tests.dll` asserts its `g_fla_timer_service` no longer holds
`fla_timer_service.c`'s abort stubs, so running that suite under a host proves the
host's timer injection reached the DLL. Under BUT it now passes 8 of 8; with the
injection reverted it reports 7 of 8.

Separately, several scripts invoke an executable by bare name after a `pushd`. With
`NoDefaultCurrentDirectoryInExePath=1` in the environment `cmd.exe` does not search the
current directory, and `setup.cmd` puts no output directory on `PATH`, so such a run
never starts. Every call site takes the `.\<name>.exe` form `all.cmd` already uses:
`but_driver.cmd`, `index.cmd` and `new.cmd` for `but_driver.exe`, and
`build_test_dll.cmd` and `malloc_cleanup_config.cmd` for `faultline.exe` — so
`<component>.cmd test` exercises its suite rather than printing an empty results table.

`but_driver.exe` returns 1 when a suite reports failures, fails to load, or is missing a
required export, and the three scripts that run it fail the build on a non-zero code.
`faultline.exe` keeps returning 0 for ordinary test failures, since it records them in
the results database; `all.cmd` checks its code to catch a crash that truncates the run.

## Open questions

- **`/EHs` vs `/EHsc` for suites.** A C++ suite's throw crosses its own `extern "C"`
  shim frame, which needs `/EHs`. The branch already hit this on the driver side
  (`flp_exception_service.c:50-51`). Confirm the `CommonCompilerFlagsCXX` bundle
  carries it.
- **Stale DLLs.** A suite without `fl_run_case` is refused with a message naming the
  missing export. Decide whether that is a hard skip (like an ABI mismatch,
  `command_run.c:453`) or a warning that falls back to the old path. A hard skip is
  simpler and nothing needs the fallback.
- **Threads.** A test that throws on a spawned thread has no enclosing `FL_TRY` on that
  thread. That is true today and this change neither fixes nor worsens it.
