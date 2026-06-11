# Service-split demo (driver + DLL)

A minimal, end-to-end demonstration of FaultLine's platform/application service
model, using three distribution packages together:

- **`memory_service`** — arena-backed memory service (no fault injection)
- **`log`** — logging service
- **`exception_service`** — exception service for the driver/DLL split

It is also a working check that the simplified package set actually composes.

## What it shows

The pattern is the same one the real FaultLine driver uses to run test-suite
DLLs (see `app/faultline/command_run.c`):

```
demo_driver.exe  (platform side)              demo_suite.dll  (application side)
  owns the arena + flp_ service impls           uses malloc/free, LOG_*, FL_TRY
  flp_log_init()                                  via the injected fla_ shims
  new_arena() + flp_init_memory_context()
  LoadLibrary("demo_suite.dll")  ───────────►   exports fla_set_log_service
  GetProcAddress("fla_set_log_service")          exports fla_set_exception_service
  GetProcAddress("fla_set_exception_service")    exports fla_set_memory_service
  GetProcAddress("fla_set_memory_service")       exports demo_run / demo_throw_uncaught
  flp_init_log_service(...)       ──inject──►
  flp_init_exception_service(...) ──inject──►
  flp_init_memory_service(...)    ──inject──►
  FL_TRY { demo_run(); demo_throw_uncaught(); }
  FL_CATCH_STR(fl_invalid_value) { ... }
```

`demo_suite.dll` contains **none** of the allocator, logger, or exception
engine. Its `malloc`/`free`, `LOG_*`, and `FL_TRY`/`FL_THROW` all call function
pointers that the driver fills in at run time. Two behaviours are demonstrated:

1. **`demo_run`** allocates a buffer from the driver's arena, logs through the
   driver's logger, and throws/catches an exception **within the DLL** (handled
   locally — `FL_CATCH` matches by pointer identity in the same module).
2. **`demo_throw_uncaught`** throws `fl_invalid_value` and does *not* catch it,
   so it propagates **across the DLL boundary** into the driver's top-level
   handler. The driver matches it with `FL_CATCH_STR` (not `FL_CATCH`) because
   each module links its own copy of the shared reason constants at distinct
   addresses — string comparison is required across the boundary.

## Why two binaries

The `flp_` (platform) and `fla_` (application) exception sources both define
`fl_throw_assertion` and cannot be linked into the same binary. The driver
compiles the `flp_` sources; the DLL compiles the `fla_` sources. This split is
exactly what the service model exists to support, which is why a faithful demo
is a driver plus a DLL rather than a single executable.

## Build and run

```
build\cmd\service_demo.cmd            :: debug x64 by default
build\cmd\service_demo.cmd release    :: forwards build options to setup.cmd
build\cmd\service_demo.cmd clean      :: remove target\service_demo\
```

The script produces the three packages, imports them into
`target\service_demo\tree\`, builds `demo_suite.dll` and `demo_driver.exe` into
`target\service_demo\bin\`, and runs the driver. All log output (from both the
driver and the DLL) goes to stdout through the one injected logger.
