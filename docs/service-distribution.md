# Service Distribution Guide
This guide covers how FaultLine's services are **packaged** for distribution and how a consumer repository **imports, updates, and removes** them. It is the counterpart to the [Service Integration Guide](service-integration-guide.md): that document explains how to *wire up* the service source once you have it; this one explains how to *get the source into another repo* and keep it current.

The distribution model is **vendored source**, not a binary SDK. A consumer ends up with FaultLine's `.c` and `.h` files checked into its own tree and compiles them as part of its own build. There is no `.lib`/`.dll` to ship and no version of MSVC to match.

## Quickstart: Exporting Services to Another Project

The steps are:

1. **Produce** the packages — run the dist script for each desired service (e.g. `build\cmd\memory_service_dist.cmd`).
2. **Import** them into the target project — run `build\dist\fl_import.ps1` for each package.
3. **Wire** the imported directory tree into the project's build.
4. **Use** the services.

The current set of services:

- **exception service**: `build\cmd\exception_service_dist.cmd`
- **memory service**, in two variants:
  - backed by an arena: `build\cmd\memory_service_dist.cmd`
  - backed by an arena plus fault injection: `build\cmd\fault_memory_service_dist.cmd`. This is the platform-side package for any binary that wants to *provide* fault-injected memory whether it injects into itself (a self-testing app) or into test-suite DLLs it loads (a custom driver).
- **logging service**: `build\cmd\log_service_dist.cmd`
- **file service**: `build\cmd\file_service_dist.cmd` (depends on the arena-backed `memory_service` package; note that `fault_memory_service` does not satisfy the dependency; the importer checks the service name)
- **timer and stopwatch service**: `build\cmd\timer_service_dist.cmd` (depends on the `exception_service` package)
- **test framework**: `build\cmd\test_framework_dist.cmd` (header-only `fl_test.h` — the `FL_TEST` / `FL_SUITE_*` / `FL_GET_TEST_SUITE` declarations a test-suite DLL publishes its cases through so the driver can exercise them; depends on the `exception_service` package)

This is the set of steps for exporting the services to another project. It focus on the memory and file services, exporting them to `<project>\third_party\faultline`, but it can be applied to the other services and your own target folder. Every step is covered in detail later in this guide; the per-service build wiring is covered in the [[Service Integration Guide]].

### 1 Produce the Packages
In this repo, from the repo root (cmd.exe) run:

```
build\cmd\memory_service_dist.cmd
build\cmd\file_service_dist.cmd
```

Each script writes a `dist\<pkg>\` that includes the package's sources, headers, and a generated `manifest.txt` recording exactly what's include.

### 2 Import Them Into the Target Project
Run the following PowerShell importer scripts from this repo; the importer scripts take any paths, so substitute your project's path for `<project>\third_party\faultline` :

```
build\dist\fl_import.ps1 -From dist\memory_service -Into <project>\third_party\faultline
build\dist\fl_import.ps1 -From dist\file_service   -Into <project>\third_party\faultline
```

Import a package's dependencies first: `file_service` declares `memory_service`, so the importer refuses to install it into a tree where `memory_service` is not already present. To add another service later, produce its package and import it into the same tree the same way. All services merge into one unified tree.

Re-running an import is also the **update** path: changed files are overwritten, files the package no longer ships are deleted, and `faultline.lock` tracks which service owns which file. Never hand-copy files into that tree. The lockfile won't know about them. (If the consumer must re-import without this repo checked out, copy `build\dist\fl_import.ps1`/`.cmd`/`.sh` and the `dist\<pkg>` directories into it; the importer is self-contained.)

### 3 Wire the Imported Tree Into the Consumer's Build
With `<root>` = `<project>\third_party\faultline`:

- Include paths: `<root>\include` always. Add `<root>\src` too when a memory package is present — its private headers (and `src\fnv\`, if shipped) live there.
- Two exclusions always apply: never compile `region_windows.c` (it is unity-included by `region_os.c`), and never compile both `flp_exception_service.c` and `fla_exception_service.c` into the same binary (they clash on the shared exception entry points).
- Pick the side per binary:
    - The project's own executable acts as the platform: build `/DFL_PLATFORM_BUILD` and compile the `fl_`/`flp_` sources plus the portable core (arena, `region_os.c`, …), dropping `fla_exception_service.c`.
    - A test-suite DLL for `faultline.exe` to load and fault-inject: build `/DDLL_BUILD` (no `FL_PLATFORM_BUILD`) and compile only the `fl_`/`fla_` sources — no `flp_*.c` and none of the arena/region sources. The driver injects the platform services at load time, so `malloc`, `FL_FILE_*`, `LOG_*`, and `FL_TRY` all route through the injected `g_fla_*` global variables.

`build\dist\selftest\fl_dist_selftest.cmd` compiles and runs a project test against every package through this exact path; treat it as the authoritative example of these rules.

### 4 Use the Services
The selector headers (`fl_memory.h`, `fl_file.h`, ...) select either the "platform" (OS dependent) or "application" (OS independent) implementation based on whether or not `FL_PLATFORM_BUILD` is defined. The [Service Integration Guide](service-integration-guide.md) lists, per service, the headers to include, the sources each side compiles, the required exports, and the injection order.

## Why not just copy the files?
The original `*_dist.cmd` scripts copied the current set of files into a `dist\`
folder, but a plain copy into a consumer repo cannot answer two questions:

1. **What is now stale?** When a service drops a file in a new version, a copy
   leaves the old file behind in the consumer, where it keeps compiling and
   diverging silently.
2. **Who owns shared files?** Several packages bundle the same shared source
   (e.g. `fl_exception_service.c`). A copy either clobbers blindly or duplicates.

The distribution system solves both with a **manifest** (the authoritative file list for a
package) and a **lockfile** (a record in the consumer of which service owns which file).
Importing is a *reconcile*, not a copy: files dropped from a package are deleted from the
consumer on update, and shared files are reference-counted so they survive until the last
owner stops shipping them.

## Pieces

### Producer side (this repo)

| File                              | Role                                                                 |
| --------------------------------- | -------------------------------------------------------------------- |
| `build/cmd/*_dist.cmd`            | Collect one service's files into `dist\<pkg>\` and emit its manifest |
| `build/dist/fl_emit_manifest.ps1` | Hash every file in a package dir and write `manifest.txt`            |

The seven packages:

| Build script                    | Package dir                 | Service name           | Depends on          | Notes                                                         |
| ------------------------------- | --------------------------- | ---------------------- | ------------------- | ------------------------------------------------------------- |
| `log_service_dist.cmd`          | `dist\log_service`          | `log_service`          | —                   | Log service, both sides                                       |
| `exception_service_dist.cmd`    | `dist\exception_service`    | `exception_service`    | —                   | Exception service, both sides (driver `flp_` + DLL `fla_`); self-contained for single binaries via `/DFL_PLATFORM_BUILD`. |
| `memory_service_dist.cmd`       | `dist\memory_service`       | `memory_service`       | —                   | Arena-only memory service (no fault injection): arena + platform exception+log + both service sides. Build `/DFL_PLATFORM_BUILD`. |
| `fault_memory_service_dist.cmd` | `dist\fault_memory_service` | `fault_memory_service` | —                   | Fault-injecting memory service: arena + fault injector + platform exception+log + app-side memory service. Build `/DFL_PLATFORM_BUILD`. For any binary that provides fault-injected memory — to itself (a self-testing app) or to suite DLLs it loads (a custom driver). |
| `timer_service_dist.cmd`        | `dist\timer_service`        | `timer_service`        | `exception_service` | Monotonic timer service, both sides, plus the `FLStopwatch` composition and the `fl_timer.h` selector. First package to use `SVC_DEPENDS`: it ships only its own files. |
| `file_service_dist.cmd`         | `dist\file_service`         | `file_service`         | `memory_service`    | Positional file service, both sides, plus the `fl_file.h` selector and the async contract sketch. The provider allocates through `FL_MALLOC`, supplied by `memory_service`. |
| `test_framework_dist.cmd`       | `dist\test_framework`       | `test_framework`       | `exception_service` | Test-declaration header (`fl_test.h`), header-only: the `FL_TEST` / `FL_SUITE_*` / `FL_GET_TEST_SUITE` macro family and the `fl_get_test_suite` export the driver enumerates a suite through. |

The first four packages are **self-contained** (they bundle the shared sources they need; reference counting makes the overlap safe). The timer and file packages instead declare dependencies: import the dependency into the same tree first, or the importer refuses (see below).

Note the package **directory** and the **service name** recorded in the manifest are set independently — a script can register under a name that differs from its directory.

### Consumer side (other repos)

| File                       | Role                                                                        |
| -------------------------- | --------------------------------------------------------------------------- |
| `build/dist/fl_import.ps1` | The importer (PowerShell). Does all the real work.                          |
| `build/dist/fl_import.cmd` | Thin `.cmd` wrapper around the `.ps1` for batch-file callers                |
| `build/dist/fl_import.sh`  | Bash port of the importer (MSYS2 / Git-Bash; needs bash 4+ and `sha256sum`) |

The three importers share the same manifest and lockfile formats and produce byte-identical results, so a repo can use whichever fits its toolchain.

## Producing a package
Run the relevant build script from the repo root. No build of the library is required. The dist scripts only copy source and headers.

```
build\cmd\fault_memory_service_dist.cmd
```

Each script:

1. **Wipes** `dist\<pkg>\src` and `dist\<pkg>\include` so the package reflects
   exactly the current file set (no leftovers from a previous run).
2. Copies the service's `.c` and `.h` files into that tree.
3. Calls `fl_emit_manifest.ps1`, which walks what is actually on disk, computes a
   SHA-256 for every file, and writes `manifest.txt` at the package root.

Because the manifest is generated from disk *after* the wipe-and-copy, it can never drift from what the script actually shipped.

`<pkg> clean` removes the package directory entirely.

### Adding, removing, or renaming a file in a package

Edit the `COPY` lines in the relevant `*_dist.cmd` and re-run it. The manifest regenerates automatically. On the next import, consumers pick up additions, overwrite changes, and **delete** anything the package no longer ships (provided no other installed service still owns that file). There is nothing else to maintain. The manifest is derived, not hand-edited.

### Versioning and dependencies

Each dist script sets three metadata variables near the top:

```
SET SVC_NAME=fault_memory_service
SET SVC_VERSION=0.2.0
SET SVC_DEPENDS=
```

* **`SVC_VERSION`** — bump on release. It is recorded in the manifest and the
  consumer lockfile and shown by `-List`.
* **`SVC_DEPENDS`** — space-separated names of other services that must already
  be installed. The first four packages are **self-contained** (they bundle the
  shared sources they need; refcounting makes the overlapping imports safe) and
  declare no dependencies. The timer, file, and test-framework packages instead
  ship only their own files and declare `exception_service`, `memory_service`,
  and `exception_service` respectively; the importer refuses to install a
  package until its dependencies are present (override with `-NoDepCheck` /
  `--no-dep-check`).

## Package layout

Every package has the same shape:

```
dist\<pkg>\
  manifest.txt              authoritative file list (name, version, depends, f <path> <sha256>)
  src\                      .c sources + private .h headers — compile these
    fnv\                    (fault_memory_service package only) FNV hash sources; FNV64.c compiles
  include\
    faultline\              public headers, included as <faultline/...>
    *.h                     a few top-level headers included as <flp_*_service.h>
```

The manifest is plain text with **LF line endings and no BOM** so it parses identically under PowerShell and bash. Do not hand-edit it.

## Importing into a consumer

Copy the `build/dist/fl_import.*` scripts and the package directory into the consumer, then run the importer. All services merge into a single unified tree under `-Into` — the "root" (default `third_party/faultline`):

```
<root>\src\...        compile these (plus src\fnv\*.c if present)
<root>\include\...     add <root>\include to the consumer's include path
<root>\faultline.lock  bookkeeping — which service owns which file
```

### PowerShell

```powershell
# Import or update a package
build\dist\fl_import.ps1 -From dist\fault_memory_service -Into third_party\faultline

# List what is installed
build\dist\fl_import.ps1 -Into third_party\faultline -List

# Remove a service (shared files kept if another service still owns them)
build\dist\fl_import.ps1 -Into third_party\faultline -Remove fault_memory_service
```

`fl_import.cmd` takes the same arguments for batch-file callers.

### Bash (MSYS2 / Git-Bash)

```bash
build/dist/fl_import.sh --from dist/fault_memory_service --into third_party/faultline
build/dist/fl_import.sh --into third_party/faultline --list
build/dist/fl_import.sh --into third_party/faultline --remove fault_memory_service
```

### What import does

For each file in the manifest it copies the file into the unified tree, then re-hashes the copy and aborts on any mismatch (corrupt package). It then:

* **Updates in place** — re-importing a package overwrites changed files.
* **Removes stale files** — files the service shipped before but no longer lists
  are deleted, unless another installed service still owns them.
* **Reference-counts shared files** — when two packages ship the same path, both
  are recorded as owners. If their contents differ, the importer prints a
  conflict warning and the most recently imported version wins. (All current
  packages ship byte-identical shared sources, so this does not arise in
  practice.)
* **Rewrites the lockfile** and prunes any directories left empty.

A typical run reports its tally:

```
Imported 'memory_service' v0.2.0 into third_party\faultline
  70 added, 0 updated, 0 removed (stale), 0 shared-file conflict(s)
```

### Importing overlapping packages

Because the self-contained packages bundle their shared sources, importing more than one into the same tree is safe — e.g. `memory_service` and `fault_memory_service` both ship `flp_exception_service.c`. The shared file lands once and gains two owners; removing either service keeps the file for the other. There is no need to import a "base" package first. (The timer and file packages are the exception: they ship only their own files, so their declared dependency must be imported into the same tree first.)

## Building the imported source

Each service has a platform (`flp_`) side and an application (`fla_`) side, selected at compile time by `FL_PLATFORM_BUILD` (through the `fl_try.h` / `fl_log.h` / `fl_memory.h` / `fl_timer.h` / `fl_file.h` selector headers). A single binary compiles only the side it needs — compiling both `flp_exception_service.c` and `fla_exception_service.c` into one binary is a link error, because both define the shared exception entry points.

**Which build mode each package targets:**

| Package | Build mode | Why |
| --- | --- | --- |
| `memory_service`, `fault_memory_service` | **`/DFL_PLATFORM_BUILD`** | Platform-side binaries — a self-testing single binary, or a custom driver that injects into suite DLLs it loads. These ship the self-contained `flp_` exception and log services, which work with no driver and no injection. |
| `exception_service`, `log_service` | either side | Ship both sides. Build a single binary `/DFL_PLATFORM_BUILD` (compiles the self-contained `flp_` side), or compile the `fla_` side into a test DLL `/DDLL_BUILD` for a driver to inject at load time — see the service-split demo (`examples/service_demo`). |
| `timer_service`, `file_service` | either side | Ship both sides plus their selector headers (`fl_timer.h`, `fl_file.h`). Their platform sides lean on the packages they declare as dependencies: `exception_service` for timer, `memory_service` (which bundles exception + log) for file. |
| `test_framework` | either side (header-only) | Nothing to compile. Suite sources include `fl_test.h` and build `/DDLL_BUILD` so `fl_get_test_suite` is exported for the driver to resolve. |

> The `memory_service` and `fault_memory_service` packages are **platform-side**: they ship `flp_exception_service.c`
> and `flp_log_service.c` (self-contained), not the `fla_` abort-stubs. Build them
> `/DFL_PLATFORM_BUILD`, or the first `FL_TRY` / `LOG_*` will hit an uninitialized service and
> abort. (The application-side memory service, `fla_memory_service.c`, is the exception: the
> memory packages still ship it, because a platform injects it into itself — see the memory
> consumer test.)

**Do not compile unity-`#include`d sources directly.** `region_os.c` `#include`s the platform implementation for the target OS (`region_windows.c` on Windows). Both files are shipped, but only `region_os.c` is compiled — compiling `region_windows.c` as its own translation unit duplicates `new_region` / `commit` / `extend_region` / … So a "compile `src\*.c`" rule must **exclude `region_windows.c`** (and any other `region_<os>.c`).

**Include paths:**

* Add `<root>\include` always (public headers, included as `<faultline/...>` and `<flp_*_service.h>`).
* For the `memory_service` and `fault_memory_service` packages, also add `<root>\src` — the private headers live alongside the sources, and that path also covers `src\fnv\` for the `fault_memory_service` package.

For the `fault_memory_service` package, compile `<root>\src\fnv\FNV64.c` along with the other `src\*.c` files.

A worked, runnable example of every rule above — build mode, include paths, and the `region_windows.c` exclusion — lives in `build\dist\selftest\` (see [Validating the packages](#validating-the-packages)).

See the [Service Integration Guide](service-integration-guide.md) for exactly which headers to include, which sources each side compiles, the required exports, and the injection order once the source is in place.

## Validating the packages

`build\dist\selftest\fl_dist_selftest.cmd` is an end-to-end self-test of the whole pipeline. For each package it runs the dist script, imports the package into an isolated scratch tree via `fl_import.ps1`, compiles a small consumer test against the *imported* tree, and runs it:

```
build\dist\selftest\fl_dist_selftest.cmd
```

It is the authoritative example of the correct consumer build for each package, and it catches packaging regressions — a missing header, a dropped source, a wrong build mode, or a unity-included source compiled by mistake. It recovers the coverage the deleted standalone tests provided: the application-side log service (`log_consumer_test.c`) and the single-binary memory-service assembly with fault injection (`memory_consumer_test.c`), plus minimal "the packaged subset compiles and runs" smoke tests for arena and the exception driver.

## Manifest and lockfile formats

For reference and debugging; both are managed by the tooling and should not be edited by hand.

**`manifest.txt`** (in each package):

```
# comment lines start with #
name <service-name>
version <x.y.z>
depends <space-separated service names, may be empty>
generated <UTC ISO-8601 timestamp>
f <relative/path/with/forward/slashes> <sha256-hex>
... one f line per file, sorted by path ...
```

**`faultline.lock`** (in the consumer's `-Into` tree):

```
# comment
svc <service-name> <version>
f <service-name> <relative/path>
... one f line per (service, file) ownership record ...
```
