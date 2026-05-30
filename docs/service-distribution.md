# Service Distribution Guide
This guide covers how FaultLine's services are **packaged** for distribution and how a consumer repository **imports, updates, and removes** them. It is the counterpart to the [Service Integration Guide](service-integration-guide.md): that document explains how to *wire up* the service source once you have it; this one explains how to *get the source into another repo* and keep it current.

The distribution model is **vendored source**, not a binary SDK. A consumer ends up with FaultLine's `.c` and `.h` files checked into its own tree and compiles them as part of its own build. There is no `.lib`/`.dll` to ship and no version of MSVC to match.

## Why not just copy the files?
The original `*_dist.cmd` scripts copied the current set of files into a `dist\`
folder, but a plain copy into a consumer repo cannot answer two questions:

1. **What is now stale?** When a service drops a file in a new version, a copy
   leaves the old file behind in the consumer, where it keeps compiling and
   diverging silently.
2. **Who owns shared files?** Several packages bundle the same shared source
   (e.g. `fl_exception_service.c`). A copy either clobbers blindly or duplicates.

The distribution system solves both with a **manifest** (the authoritative file list for a package) and a **lockfile** (a record in the consumer of which service owns which file). Importing is a *reconcile*, not a copy: files dropped from a package are deleted from the consumer on update, and shared files are reference-counted so they survive until the last owner stops shipping them.

## Pieces

### Producer side (this repo)

| File                              | Role                                                                 |
| --------------------------------- | -------------------------------------------------------------------- |
| `build/cmd/*_dist.cmd`            | Collect one service's files into `dist\<pkg>\` and emit its manifest |
| `build/dist/fl_emit_manifest.ps1` | Hash every file in a package dir and write `manifest.txt`            |

The five packages:

| Build script                     | Package dir               | Service name       | Notes                                                         |
| -------------------------------- | ------------------------- | ------------------ | ------------------------------------------------------------- |
| `exception_sa_dist.cmd`          | `dist\exception`          | `exception`        | Exception service, both sides                                 |
| `log_sa_dist.cmd`                | `dist\log`                | `log`              | Log service, both sides                                       |
| `arena_sa_dist.cmd`              | `dist\arena`              | `arena`            | Standalone arena; bundles self-contained platform (`flp_`) exception+log. Build `/DFL_BUILD_DRIVER`. |
| `flp_memory_service_sa_dist.cmd` | `dist\flp_memory_service` | `memory`           | Memory service: arena + fault injector + platform exception+log + app-side memory service. Build `/DFL_BUILD_DRIVER`. |
| `exception_driver_dist.cmd`      | `dist\exception-driver`   | `exception-driver` | Platform/driver exception package; self-contained for single binaries. Build `/DFL_BUILD_DRIVER`. |

Note the package **directory** and the **service name** recorded in the manifest can differ — the memory package lives in `dist\flp_memory_service\` but registers as `memory`.

### Consumer side (other repos)

| File                       | Role                                                                        |
| -------------------------- | --------------------------------------------------------------------------- |
| `build/dist/fl_import.ps1` | The importer (PowerShell). Does all the real work.                          |
| `build/dist/fl_import.cmd` | Thin `.cmd` wrapper around the `.ps1` for batch-file callers                |
| `build/dist/fl_import.sh`  | Bash port of the importer (MSYS2 / Git-Bash; needs bash 4+ and `sha256sum`) |

The three importers share the same manifest and lockfile formats and produce byte-identical results, so a repo can use whichever fits its toolchain.

## Producing a package
Run the relevant build script from the repo root. No build of the library is required — the dist scripts only copy source and headers.

```
build\cmd\flp_memory_service_sa_dist.cmd
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

Edit the `COPY` lines in the relevant `*_dist.cmd` and re-run it. The manifest regenerates automatically. On the next import, consumers pick up additions, overwrite changes, and **delete** anything the package no longer ships (provided no other installed service still owns that file). There is nothing else to maintain — the manifest is derived, not hand-edited.

### Versioning and dependencies

Each dist script sets three metadata variables near the top:

```
SET SVC_NAME=memory
SET SVC_VERSION=0.2.0
SET SVC_DEPENDS=
```

* **`SVC_VERSION`** — bump on release. It is recorded in the manifest and the
  consumer lockfile and shown by `-List`.
* **`SVC_DEPENDS`** — space-separated names of other services that must already
  be installed. All current packages are **self-contained** (they bundle the
  shared sources they need) and declare no dependencies, so refcounting makes
  overlapping imports safe. If you split a package so it relies on another,
  list the dependency here and the importer will refuse to install it until the
  dependency is present (override with `-NoDepCheck` / `--no-dep-check`).

## Package layout

Every package has the same shape:

```
dist\<pkg>\
  manifest.txt              authoritative file list (name, version, depends, f <path> <sha256>)
  src\                      .c sources + private .h headers — compile these
    fnv\                    (memory package only) FNV hash sources; FNV64.c compiles
  include\
    faultline\              public headers, included as <faultline/...>
    *.h                     a few top-level headers included as <flp_*_service.h>
```

The manifest is plain text with **LF line endings and no BOM** so it parses identically under PowerShell and bash. Do not hand-edit it.

## Importing into a consumer

Copy the `build/dist/fl_import.*` scripts and the package directory into the consumer, then run the importer. All services merge into a single unified tree under `-Into` (default `third_party/faultline`):

```
<Into>\src\...        compile these (plus src\fnv\*.c if present)
<Into>\include\...     add <Into>\include to the consumer's include path
<Into>\faultline.lock  bookkeeping — which service owns which file
```

### PowerShell

```powershell
# Import or update a package
build\dist\fl_import.ps1 -From dist\flp_memory_service -Into third_party\faultline

# List what is installed
build\dist\fl_import.ps1 -Into third_party\faultline -List

# Remove a service (shared files kept if another service still owns them)
build\dist\fl_import.ps1 -Into third_party\faultline -Remove memory
```

`fl_import.cmd` takes the same arguments for batch-file callers.

### Bash (MSYS2 / Git-Bash)

```bash
build/dist/fl_import.sh --from dist/flp_memory_service --into third_party/faultline
build/dist/fl_import.sh --into third_party/faultline --list
build/dist/fl_import.sh --into third_party/faultline --remove memory
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
Imported 'memory' v0.2.0 into third_party\faultline
  70 added, 0 updated, 0 removed (stale), 0 shared-file conflict(s)
```

### Importing overlapping packages

Because packages are self-contained, importing more than one into the same tree is safe — e.g. `arena` and `memory` both ship `flp_exception_service.c`. The shared file lands once and gains two owners; removing either service keeps the file for the other. There is no need to import a "base" package first.

## Building the imported source

Each service has a platform (`flp_`) side and an application (`fla_`) side, selected at compile time by `FL_BUILD_DRIVER` (through the `fl_try.h` / `fl_log.h` / `fl_memory.h` selector headers). A single binary compiles only the side it needs — compiling both `flp_exception_service.c` and `fla_exception_service.c` into one binary is a link error, because both define the shared exception entry points.

**Which build mode each package targets:**

| Package | Build mode | Why |
| --- | --- | --- |
| `exception-driver`, `arena`, `memory` | **`/DFL_BUILD_DRIVER`** | Single-binary / platform consumers. These ship the self-contained `flp_` exception and log services, which work with no driver and no injection. |
| `exception`, `log` | application/DLL (`/DDLL_BUILD`, no `FL_BUILD_DRIVER`) | Compiled into a test DLL whose services a driver injects at load time. |

> The `arena` and `memory` packages are **platform-side**: they ship `flp_exception_service.c`
> and `flp_log_service.c` (self-contained), not the `fla_` abort-stubs. Build them
> `/DFL_BUILD_DRIVER`, or the first `FL_TRY` / `LOG_*` will hit an uninitialized service and
> abort. (The application-side memory service, `fla_memory_service.c`, is the exception: the
> memory package still ships it, because a platform injects it into itself — see the memory
> consumer test.)

**Do not compile unity-`#include`d sources directly.** `region_os.c` `#include`s the platform implementation for the target OS (`region_windows.c` on Windows). Both files are shipped, but only `region_os.c` is compiled — compiling `region_windows.c` as its own translation unit duplicates `new_region` / `commit` / `extend_region` / … So a "compile `src\*.c`" rule must **exclude `region_windows.c`** (and any other `region_<os>.c`).

**Include paths:**

* Add `<Into>\include` always (public headers, included as `<faultline/...>` and `<flp_*_service.h>`).
* For the `arena` and `memory` packages, also add `<Into>\src` — the private headers live alongside the sources, and that path also covers `src\fnv\` for the memory package.

For the memory package, compile `<Into>\src\fnv\FNV64.c` along with the other `src\*.c` files.

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
