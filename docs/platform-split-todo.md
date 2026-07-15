# Platform/Core Split — Outstanding Service Work

Pick-up checklist for services conforming to the provider/consumer
(`flp_`/`fla_`) + portable-contract (`fl_`) layout that the log, exception, and
memory services follow. See **Two architectural axes** and the **Exception/Log
Service** sections in `CLAUDE.md` for the target pattern.

Status: **complete (2026-07-14).** A (timer), B (file service), and C (module
service + split assembly) are done, including all four seams, the
`win32_faultline.exe` naming, and continuous build/test of the split in both
`all.cmd` and `all.sh`. What's left below is deferred/optional work only (dist
packages, async file service, seek/tell/flush contract rev).

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
- [x] `timer_service` dist package (2026-07-14): `timer_service_dist.cmd`, the
  first package to use `SVC_DEPENDS` (depends on `exception_service` instead of
  bundling it). Covered by `fl_dist_selftest.cmd`'s consumer-mode injection test.
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
- [x] `file_service` / `timer_service` dist packages (2026-07-14):
  `file_service_dist.cmd` depends on `memory_service` (which now also ships the
  `fl_memory.h` selector the provider compiles against); `timer_service_dist.cmd`
  depends on `exception_service`. Both covered by `fl_dist_selftest.cmd`.

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

### Split exe name + continuous build ✅ done (2026-07-14)
- ✅ The split exe is `win32_faultline.exe` (matching the `win32_faultline_*.c`
  host sources); the monolith keeps `faultline.exe`. The two builds no longer
  overwrite each other.
- ✅ `all.cmd` builds the split (after `std_faultline.cmd`) and its test step runs
  a split smoke — driver / file-service / timer suites through
  `win32_faultline.exe` — after the monolith run, so every `all.cmd test`
  exercises both hosts. Verified: monolith 23 suites 100%; smoke 3 suites 100%
  with 6 faults injected across the DLL boundary.

### Seam #2 (allocation) ✅ resolved (2026-07-14) — by moving the line, not the call
Routing the driver's one allocation through the injected memory service was rejected:
the platform memory service is itself arena-backed (`flp_malloc` == `arena_malloc` on
the same arena), the arena is the core's pervasive substrate (buffers, summaries, the
injector's own API take `Arena *`), and the swap would have traded an explicit
parameter for an ambient global. The arena is portable core code by the project's own
axis-1 doctrine; what's OS-specific is its *paging backing*. So:
- The portable memory stack (`region.c`, `region_node.c`, `arena.c`, `arena_dbg.c`,
  `arena_malloc.c`) moved from the platform unity into `faultline_core_unity.c`; the
  platform TU keeps only `region_os.c` (the per-OS paging dispatch). A port
  reprovides `region_<os>.c` and keeps the stack — exactly the documented reuse story.
- The driver's alloc+init pair collapsed to the existing `fault_injector_create()`;
  `faultline_driver.c` no longer includes `arena.h`.
- **Bootstrap-injection gotcha (matters for ports):** the core-compiled arena logs and
  throws through the core's embedded `g_fla_*` services, which are default-abort stubs
  until installed. The host must call `flp_init_log_service(fla_set_log_service)` and
  `flp_init_exception_service(fla_set_exception_service)` *before* its first core call
  (`new_arena`); `faultline_app_main` installs the full set later. The host TU compiles
  with `/DFL_EMBEDDED` so the linked core's plain (non-dllimport) setters match.
- Verified: split `faultline_split.cmd test` 27/27; `std_faultline` builds;
  `all.cmd test` green (monolith 23 suites 100%, split smoke 3 suites 100%).
### Seam #4 (JUnit output) ✅ done (2026-07-14)
`output_junit.c` writes through the file service (`FL_FILE_OPEN/WRITE/CLOSE` via the
`fl_file.h` selector) instead of `fopen_s`/`fprintf`. A small buffered accumulator
(`JUnitOut`, local to the file) batches the short XML fragments into one service
write per KB and tracks the running offset, since the service has no formatted-output
primitive and its writes are positional; `out_printf` handles bounded content
(numbers, timestamps) while unbounded strings go through the chunking
`out_puts`/`xml_write_escaped`. `junit_begin` opens with `FL_FILE_WRITE`
(create/truncate), replacing the old `remove()`+append — deletion never entered the
contract. `JUnitXML.file` is now `FLFile *`. Verified: both hosts emit structurally
identical, well-formed XML; rewriting an existing file truncates cleanly;
`all.cmd test` green (monolith 23 suites + split smoke, 100%).

### Bash/clang counterparts ✅ done (2026-07-14)
- ✅ `build/bash/faultline_core.sh` compiles `faultline_core_unity.c` (no
  `FL_PLATFORM_BUILD`, with `FL_EMBEDDED`) to `faultline_core.o` — the clang build
  links objects directly, so the object is the unit where the cmd build archives
  `faultline_core.lib`.
- ✅ `build/bash/faultline_split.sh` builds `win32_faultline.exe`
  (`win32_faultline_unity.c` + `faultline_core.o` + shared `sqlite3.o`/`cwalk.o`),
  with the same guards as the cmd script: rebuild fixtures only when missing,
  forward filtered args to sub-builds (no nested re-clean), and rebuild
  `faultline_tests.dll` via `faultline.sh` when testing after a clean.
- ✅ `all.sh` builds the split after `faultline.sh` and runs the same
  driver/file-service/timer smoke through `win32_faultline.exe` as `all.cmd`.
- ✅ Verified: `faultline_split.sh test` and `test clean` both 27/27;
  `all.sh test` green (monolith 23 suites 100%, split smoke 3 suites 100% with
  6 faults injected across the DLL boundary).

### Remaining
None — the platform/core split work is complete, and the timer/file dist
packages shipped 2026-07-14. Deferred/optional items live in their sections
above (the async file service contract, seek/tell/flush/size/stat, a
fault-injecting file service).
