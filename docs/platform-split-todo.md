# Platform/Core Split — Outstanding Service Work

Pick-up checklist for services conforming to the provider/consumer
(`flp_`/`fla_`) + portable-contract (`fl_`) layout that the log, exception, and
memory services follow. See **Two architectural axes** and the **Exception/Log
Service** sections in `CLAUDE.md` for the target pattern.

Status: **A (timer) is done** and now serves as the worked example — it added a
selector and cross-boundary injection that **B (file service)** can copy. B is
the remaining blocker for wiring the platform/core split into `all.cmd`/`all.sh`.

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
  side (`FL_PLATFORM_BUILD`), `g_fla_timer_service.*` on the consumer side. No
  `FL_TIMER_SERVICE`/`FL_STOPWATCH` surface — the selector resolves *calls* only.
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

## B. `fl_file_service.h` — file-service contract (greenfield)

Barely exists: only `platform_api.h`'s references. Blocks wiring the platform/core
split into `all.cmd`/`all.sh`.

### Current state
- ❌ `include/faultline/fl_file_service.h` does not exist, yet `platform_api.h:20`
  includes it and `:72` declares `FLFileService *file`.
- ❌ `src/flp_file_service.c` does not exist (referenced in
  `win32_faultline_unity.c:19` comment and `win32_faultline_main.c:26` prereqs).
- ❌ No `flp_file_service.h` / `fla_file_service.h`.
- ✅ Only the intent exists: `win32_faultline_main.c:110–114` assembles
  `FLFileService file = { .open=flp_file_open, .read=flp_file_read,
  .write=flp_file_write, .close=flp_file_close }`.

This is why the platform/core split (`win32_faultline_main.c`, `platform_api.h`,
`faultline_core`) is not yet wired into `all.cmd` — it can't compile until this lands.

### Design decisions to settle first (block the contract)
- [ ] Opaque handle: `typedef struct FLFile FLFile;` wrapping a Win32 `HANDLE`
  (and `FILE*`/`fd` on other hosts). Never exposed to consumers.
- [ ] `open` signature & mode: path + a portable mode enum/flags (read/write/append,
  create/truncate, text/binary). Returns `FLFile*` or `NULL`.
- [ ] Error model: does `open` failure return `NULL` or throw via the exception
  service? Do `read`/`write` return bytes transferred or status + out-param?
  Recommend return-bytes + `NULL`/short-count, keeping the contract free of an
  exception-service dependency so it stays drop-in; the *caller* decides whether to throw.
- [ ] Scope of v1: open/read/write/close only (matching `platform_api.h`). Defer
  seek/tell/flush/size/stat to a later rev — note but don't block.
- [ ] Fault-injection: out of scope for v1. Note as a future parallel to the
  fault-*memory* service (a `flp_fault_file_service` injected only into suites) if
  I/O failure injection is ever wanted.

### Checklist (mirror the log/exception layout)
- [ ] `include/faultline/fl_file_service.h` (contract): `FLFile` opaque;
  `FL_FILE_OPEN_FN`/`FL_FILE_READ_FN`/`FL_FILE_WRITE_FN`/`FL_FILE_CLOSE_FN`
  typedefs; `FLFileService` struct of those pointers; `FLA_SET_FILE_SERVICE_FN` +
  `FLA_SET_FILE_SERVICE_STR`. Neutral vocabulary (caller/consumer); `#include <stddef.h>`
  for `size_t`.
- [ ] `include/flp_file_service.h` (provider): declare `flp_file_open/read/write/close`
  and (optionally) `flp_init_file_service` / `FLP_INIT_FILE_SERVICE_FN`.
- [ ] `include/faultline/fla_file_service.h` (consumer accessor):
  `g_fla_file_service`, `fla_set_file_service`.
- [ ] `src/flp_file_service.c` (Win32 impl): `CreateFileA`/`ReadFile`/`WriteFile`/
  `CloseHandle`; `FLFile` wraps `HANDLE`.
- [ ] `src/fla_file_service.c` (consumer TLS service + default-abort/stub writers +
  `fla_set_file_service`).
- [ ] No `fl_file.h` selector unless file convenience macros are added — likely not;
  called through the struct like the timer.
- [ ] `win32_faultline_main.c`: add `#include <flp_file_service.h>` (assembly at
  110–114 already exists).
- [ ] `win32_faultline_unity.c`: already references `flp_file_service.c` — confirm
  the include lands once the file exists.
- [ ] Build scripts (`.cmd` **and** `.sh`): compile `flp_file_service.c` /
  `fla_file_service.c`; add a `file_service` dist package if the other services ship one.
- [ ] Wire the platform/core split (`win32_faultline_main.c` + `faultline_core`) into
  `all.cmd`/`all.sh` once it compiles, so it's actually built and tested.
- [ ] Add `src/fl_file_service_tests.c` (round-trip write→read→close on a temp file,
  error paths), following `fl_log_service_tests.c`. Include a cross-boundary
  injection test (assert the driver replaced the default stubs, then exercise the
  injected service) like the timer/log/memory suites now have.
- [ ] Verify both builds green.

---

## Suggested order

1. ~~**A** (timer provider header)~~ — done; proved the pattern (and added the
   selector + injection the file service can now copy).
2. **B** design decisions (error model + open mode are the crux).
3. **B** contract → provider/consumer headers → impls → tests → build wiring.
   Mirror the timer's finished shape: contract `fl_*` + `fla_*` in
   `include/faultline/`, provider `flp_*` in `include/`, optional `fl_file.h`
   selector only if file convenience macros are wanted (timer added one; file
   likely doesn't need it), injection in `command_run.c` + `faultline_app_main.c`.
