# Building with Clang / MSYS2

A Clang-based build is provided alongside the primary MSVC build. Its main purpose is
to generate a `compile_commands.json` file, which enables
[Include What You Use (IWYU)](https://include-what-you-use.org/) analysis.

## Prerequisites

| Tool                  | Notes                                                                                   |
| --------------------- | --------------------------------------------------------------------------------------- |
| **LLVM** (tested: 21) | Must include **clang** and **lld** — see below                                          |
| **MSYS2 / MINGW64**   | Provides the MinGW-w64 sysroot (headers, standard libraries, `x86_64-w64-mingw32-gcc`) |
| **Bash**              | MSYS2's MINGW64 shell                                                                   |

### Building LLVM with the required components

LLD is not included in a default LLVM build. Without it, release builds fail because
`-flto` produces LLVM bitcode objects that GNU `ld` cannot link. Build LLVM from source
with at minimum:

```
cmake -DLLVM_ENABLE_PROJECTS="clang;lld" ^
      -G "Visual Studio 17 2022" -A x64 -Thost=x64 ..\llvm
```

Install to a versioned prefix (e.g., `C:\Users\<you>\llvm21`) and add its `bin\`
directory to your `PATH`.

### Optional: `compiler-rt` for thread-local storage

`FL_THREAD_LOCAL` is currently disabled for clang+MinGW because the native Windows TLS
implementation is unreliable without LLVM's `compiler-rt` runtime, and MinGW's emulated
TLS (`-femulated-tls`) depends on pthreads which are not available here.

To restore full TLS support, rebuild LLVM adding:

```
-DLLVM_ENABLE_RUNTIMES="compiler-rt" ^
-DCOMPILER_RT_DEFAULT_TARGET_TRIPLE="x86_64-w64-mingw32"
```

Then in `build/bash/config.sh`, add `-rtlib=compiler-rt` to `COMMON_LINKER_FLAGS` and
remove the no-op guard in `include/faultline/fl_macros.h`. Until then, all tests run
single-threaded and the missing TLS has no practical effect.

## Building

```bash
# debug (default)
bash build/bash/all.sh build

# release
bash build/bash/all.sh build release

# build and run all tests
bash build/bash/all.sh test

# clean
bash build/bash/all.sh clean
bash build/bash/all.sh cleanall
```

`config.sh` looks for clang at `C:/Users/<you>/llvm21/bin/clang.exe` and falls back to
whichever `clang` is on `PATH`. Edit the path variables near the top of `config.sh` if
your LLVM install is in a different location.

## Build output

```
target/clang/
  x64/
    debug/
      bin/   .dll, .exe
      lib/   import libraries (where generated)
      obj/   .o files and per-TU .json fragments
    release/
      ...
  compile_commands.json   assembled after a full build
```

`compile_commands.json` is assembled from per-translation-unit `.json` fragments emitted
by `-MJ` during compilation.

## IWYU analysis

Pass the compile commands database to `iwyu_tool.py`:

```bash
python3 /path/to/iwyu_tool.py -p target/clang/compile_commands.json src/arena.c
```

IWYU group files live in `build/bash/iwyu/`. Each defines `IWYU_SOURCES` and
`IWYU_EXTRA_FLAGS` for a component. After adding or removing group files, regenerate the
database:

```bash
bash build/bash/iwyu_db.sh
```

Stale `.json` fragments in `target/clang/x64/debug/obj/iwyu/` persist across runs;
deleting a group file does not purge its fragments until `iwyu_db.sh` is re-run.
