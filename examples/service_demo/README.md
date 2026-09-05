# Service-split demo (driver + DLL)

A minimal, end-to-end demonstration of FaultLine's platform/application service
model, using three distribution packages together:

- **`memory_service`** — arena-backed memory service (no fault injection)
- **`log`** — logging service
- **`exceptions`** — both sides compile the exception implementation

It is also a working check that the simplified package set actually composes.

## What it shows

The pattern is the same one the real FaultLine driver uses to run test-suite
DLLs (see `app/faultline/command_run.c`):

```
demo_driver.exe  (platform side)              demo_suite.dll  (application side)
  owns the arena + flp_ service impls           uses malloc/free, LOG_*, FL_TRY
  flp_log_init()                                  via the injected fla_ shims
  new_arena() + flp_init_memory_context()
  LoadLibrary("demo_suite.dll")  ----------->   exports fla_set_log_service
  GetProcAddress("fla_set_log_service")          exports fla_set_memory_service
  GetProcAddress("fla_set_memory_service")       exports demo_run
  flp_init_log_service(...)    --inject-->       exports demo_throw_at_boundary
  flp_init_memory_service(...) --inject-->
  demo_run(); demo_throw_at_boundary();
```

`demo_suite.dll` contains **neither** the allocator nor the logger: its
`malloc`/`free` and `LOG_*` call function pointers the driver fills in at run
time. Exceptions are different — they are not a service, so the DLL compiles
`fl_exception.c` and gets its own environment stack. Two behaviours are
demonstrated:

1. **`demo_run`** allocates a buffer from the driver's arena, logs through the
   driver's logger, and throws/catches an exception **within the DLL** (handled
   locally — `FL_CATCH` matches by pointer identity in the same module).
2. **`demo_throw_at_boundary`** throws `fl_invalid_value` and catches it at the
   export the driver calls through, returning the outcome. Nothing unwinds out
   of a module: an escaping throw would abort in the DLL rather than reach the
   driver's `FL_TRY`, because each module unwinds on its own stack.

## Why two binaries

The demo needs two modules to show services being injected across a real
`LoadLibrary` boundary, and to show that a DLL's exceptions stay inside the DLL.
Neither is observable in a single executable.

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
