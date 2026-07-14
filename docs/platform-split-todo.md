# Platform/Core Split — Outstanding Service Work

Pick-up checklist for services conforming to the provider/consumer
(`flp_`/`fla_`) + portable-contract (`fl_`) layout that the log, exception, and
memory services follow. See **Two architectural axes** and the **Exception/Log
Service** sections in `CLAUDE.md` for the target pattern.

Status: **A (timer) and B (file service) are done**, and **C (module service +
split assembly)** now builds and runs: `faultline_split.cmd` produces a working
split driver. What remains is C's tail — seams #1/#2/#4, build-script polish,
and wiring the split into `all.cmd`/`all.sh`.

Header location convention: `fl_*_service.h` / `fla_*_service.h` live in
`include/faultline/`; the `flp_*_service.h` provider headers live in `include/`
(root), matching `flp_log_service.h` and `flp_exception_service.h`.

---

## A. Timer service — provider header, selector, injection ✅ (essentially done)

Completed. The timer now follows the full provider/consumer/contract layout, and
went **beyond** the original "provider header only" scope below.

### Decision (resolved)
The earlier tentative recommendation was to wire the timer inline and skip a
selector ("it's a leaf with no convenience macros"). **Resolved the other way:**
added `flp_init_timer_service` / `FLP_INIT_TIMER_SERVICE_FN` *and* an `fl_timer.h`
selector, so a host can inject the clock into a suite/plugin DLL and that code can
time itself ambiently via `FL_NOW` / `FL_ELAPSED`. The deciding case was a
mock/deterministic clock injected into a DLL under test; the selector earns its
keep because `g_fla_timer_service` now has a reader.

### Current state
- ✅ Contract: `include/faultline/fl_timer_service.h`
- ✅ Consumer accessor: `include/faultline/fla_timer_service.h`
  (`g_fla_timer_service`, `fla_set_timer_service`)
- ✅ Portable composition: `include/faultline/fl_stopwatch.h`
- ✅ Platform impl: `src/flp_timer_service.c` — `flp_timer_now`,
  `flp_timer_elapsed_seconds`, static `g_timer_service`, `flp_init_timer_service`
- ✅ Provider header: `include/flp_timer_service.h` — declares the two funcs plus
  `FLP_INIT_TIMER_SERVICE_FN` / `flp_init_timer_service`, with `extern "C"` guards
  matching the sibling `flp_` headers. `flp_timer_service.c` includes it (IWYU).
- ✅ Selector: `include/faultline/fl_timer.h` — `FL_NOW` / `FL_ELAPSED`, a
  one-to-one analog of `fl_log.h`: direct calls to `flp_timer_*` on the platform
  side (`FL_PLATFORM_BUILD`), `g_fla_timer_service.*` on the consumer side. The
  original "calls only" scoping was revisited during seam #1 (see C below):
  `FL_TIMER_SERVICE()` now also names the active service instance on either side,
  so `FLStopwatch` can bind to it in dual-built TUs.
- ✅ Injection wired: `command_run.c` resolves `FLA_SET_TIMER_SERVICE_STR` and
  calls `flp_init_timer_service` per loaded suite (NULL-guarded);
  `faultline_app_main.c` installs `platform->timer` into the core's
  `g_fla_timer_service`.
- ✅ Host include: `app/faultline/win32_faultline_main.c:50` includes
  `<flp_timer_service.h>` (no longer relies on unity-only visibility).
- ✅ Build (`.cmd`): `flp_timer_service.c` added to the unity TU
  `main_unity_windows.c` and to the `std_faultline.cmd` driver source list.
- ✅ Build (`.sh`): covered transitively — `faultline.sh` compiles the unity TU
  `main_unity_windows.c`, which now `#include`s `flp_timer_service.c`.
- ✅ Tests: `src/timer_tests.c` extended — provider contract tests (now-monotonic,
  zero-elapsed, elapsed-over-sleep), a cross-boundary injection test that does *not*
  self-inject but relies on the driver resolving the exported `fla_set_timer_service`
  and installing the host provider into `g_fla_timer_service` (it asserts the default
  abort stubs were replaced, then exercises `FL_NOW`/`FL_ELAPSED`), and the stopwatch
  test repointed at the real provider (the duplicate inline `test_clock` and its
  stale comment removed). `build\cmd\timer.cmd test` → 8/8 pass.

### Remaining
- [x] Verify the clang/bash builds end-to-end: `./build/bash/timer.sh` (the timer
  suite's new white-box includes `flp_timer_service.c` / `fla_timer_service.c` must
  resolve under `-I include -I src`) and `./build/bash/all.sh test`.
- [x] Legacy triage: `src/win_timer.c`, `src/time_timer.c`, `src/tick_timer.c` are
  the older v0.2 `fl_timer.lib` implementations — decide whether to retire them.
- [ ] Optional: a `timer_service` dist package parallel to the `log_service` /
  `memory_service` dist packages, if the timer is to ship standalone for reuse.
- [x] The `std_faultline.cmd` **test DLL** (consumer side) now compiles
  `fla_timer_service.c`, so the std-built suite exports `fla_set_timer_service` for
  symmetry with the other consumer accessors it already links.
- [x] Cross-boundary injection now has real coverage: the timer, log, and memory
  suites each have a test that does *not* self-inject and asserts the driver
  replaced the default abort stubs — proving the `fla_set_*_service` symbol is
  exported and the service crossed the DLL boundary (not just that the setter
  composes in-process). The exception service needs no such test: it is required of
  every suite, so the whole run depends on its cross-boundary injection already.

---

## B. `fl_file_service.h` — file-service contract ✅ (done)

Completed, beyond the original scope. Contract (`fl_file_service.h`,
`fl_file_types.h`), provider (`flp_file_service.h`, `flp_file_service.c` — Win32
`CreateFileW` with UTF-8→UTF-16 conversion and extended-length path support),
consumer accessor (`fla_file_service.h`, `fla_file_service.c`), an `fl_file.h`
selector, an async contract sketch (`fl_async_file_service.h`), tests
(`flp_file_service_tests.c` — 10/10 incl. cross-boundary injection, 6 fault
sites), and build wiring (`file_service.cmd` / `file_service.sh`, run by
`all.cmd` / `all.sh`; `std_faultline.cmd` compiles the provider). Injection is
wired in `command_run.c`; `faultline_app_main` installs `platform->file`.

### Deferred (noted, not blocking)
- [ ] seek/tell/flush/size/stat — later rev of the contract.
- [ ] Fault-injecting file service (`flp_fault_file_service`, parallel to fault
  memory) if I/O failure injection is ever wanted.
- [ ] Optional `file_service` / `timer_service` dist packages parallel to the
  `log_service` / `memory_service` ones.

---

## C. Module service + split assembly ✅ builds and runs (tail remains)

### Seam #1 (timing) ✅ done (2026-07-13)
`faultline_driver.c` no longer times via `start_win`/`elapsed_win_seconds`; it uses
`FLStopwatch` bound to the active timer service. Getting there exposed a service-design
gap: the consumer side had an ambient instance (`g_fla_timer_service`) but the platform
provider kept its instance static, exporting only bare functions — so `FLStopwatch`
(which binds a service *pointer*) couldn't be used in a dual-built TU. Fixed by making
the active service a first-class value on both sides: `flp_timer_service()` returns the
provider's instance, and `fl_timer.h` gained `FL_TIMER_SERVICE()` (→ `flp_timer_service()`
under `FL_PLATFORM_BUILD`, `&g_fla_timer_service` otherwise). The driver binds with
`fl_stopwatch_make(FL_TIMER_SERVICE())` — note a zero-initialized `FLStopwatch` has a
NULL service pointer, so watches are made (not `{0}`-reset), preserving the
"never-started reads 0 elapsed" behavior. Fixture wiring followed the driver:
`faultline_test_data.c` gained `flp_timer_service.c` (platform side) and
`faultline_tests_unity.c` gained `fla_timer_service.c` (consumer side); `win_timer.c`
stays in both only for its `nanosleep`/`gettimeofday` polyfills. Also fixed a
pre-existing bad include in `win32_faultline_main.c` (`faultline/macros.h` →
`faultline/fl_macros.h`) that had been blocking `faultline_split.cmd`. Verified: split
driver ran timer / driver / file-service suites (100%, non-zero timings, 6 faults);
monolith `all.cmd test` green (23 suites, 100%).

### Module service + assembly. Done (2026-07-11):
- ✅ `include/flp_module_service.h` + `src/flp_module_service.c`: `flp_load_module`
  / `flp_resolve_symbol` / `flp_unload_module` (wrap `LoadLibraryA` /
  `GetProcAddress` / `FreeLibrary`; opaque `FLModule` *is* the `HMODULE`) and
  `flp_inject_services` (the full fla_set_* resolution dance, fault-memory context
  bound via `flp_module_service_init`).
- ✅ `command_run.c` seam: suite mechanics behind `suite_load` / `suite_inject` /
  `suite_symbol` / `suite_unload` / `suite_injector` — direct Win32 + `flp_init_*`
  under `FL_PLATFORM_BUILD` (monolith), `ectx->platform->*` otherwise (split core).
- ✅ `ExecutionContext.platform` added; `faultline_app_main` fills it and installs
  all five services (file included) into the core's `fla_` globals.
- ✅ Core unity gained `fla_timer_service.c` / `fla_file_service.c`;
  `faultline_core.cmd` compiles with `/DFL_EMBEDDED` (fla setters are embedded,
  not dllimport).
- ✅ One-definition fix: `flp_exception_service.c` guards `fl_throw_assertion`
  behind `!defined(FLP_OMIT_FL_THROW_ASSERTION)`; the split host defines it so the
  consumer-side copy (via injected service) is the one linked.
- ✅ Verified: `faultline_split.cmd` builds; split driver ran timer / assert /
  file-service suites (100%, faults injected); full monolith `all.cmd test` green
  (23 suites, 100%).

### Remaining
- [ ] Seam #2: driver still allocates via `arena_malloc_throw` on the platform arena.
- [ ] Seam #4: `output_junit.c` writes via `fopen`/`fprintf` (portable CRT, so it
  links, but should route through `FLFileService`).
- [ ] `faultline_split.cmd` emits `/Fe:faultline.exe` — same name/path as the
  monolith's output, so each build overwrites the other. Decide the split exe's
  name (script comments say `win32_faultline.exe`).
- [ ] Bash counterparts: no `faultline_core.sh` / `faultline_split.sh` yet.
- [ ] Wire the split into `all.cmd`/`all.sh` (after the name collision is settled)
  so it's built and tested continuously.
