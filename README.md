# FaultLine

![Build Status](https://github.com/dbc60/faultline/workflows/CI/badge.svg)
![License](https://img.shields.io/github/license/dbc60/faultline)
![GitHub Release](https://img.shields.io/github/v/release/dbc60/faultline?include_prereleases)
![GitHub issues](https://img.shields.io/github/issues/dbc60/faultline)

FaultLine is a fault-injection unit testing framework for C. It extends the
[Basic Unit Test](https://github.com/dbc60/but) framework with systematic fault
injection for `malloc`/`free`-based allocations, so you can verify that your code
handles allocation failures correctly and releases all memory even when things go wrong.

Test suites are compiled as DLLs and loaded at runtime by `faultline.exe`. FaultLine
drives each test case repeatedly, failing a different allocation on each pass, until
every code path through the allocator has been exercised. Results — pass, fail,
exception — are optionally recorded in a SQLite database for later analysis.

## Requirements

- Windows (x64 or x86)
- Visual Studio 2022 — or Clang with MSYS2/MINGW64
  (see [Building with Clang](docs/building-with-clang.md))

## Getting Started

### 1. Build FaultLine

From a plain **Windows command prompt**:

```cmd
build\cmd\all.cmd             :: debug build (default)
build\cmd\all.cmd release     :: optimized release build
build\cmd\all.cmd test        :: build and run all tests
```

From an **MSYS2/MINGW64 bash shell**:

```bash
bash build/bash/all.sh          # debug build
bash build/bash/all.sh release  # release build
bash build/bash/all.sh test     # build and run all tests
```

### 2. Install the SDK

After building, install the public headers, static library, and test driver to a
directory of your choice:

```powershell
.\build\ps\install-sdk.ps1 -Prefix C:\libs
```

This creates `C:\libs\FaultLine\` with the following layout:

```
FaultLine\
  include\faultline\         public headers
  lib\faultline.lib          static library your test DLLs link against
  lib\cmake\Faultline\       CMake find_package() support
  bin\faultline.exe          test driver
```

Run `Get-Help .\build\ps\install-sdk.ps1 -Full` for options (toolchain, platform,
build type).

### 3. Write a Test Suite

Test suites are DLLs. Each exports one function, `fl_get_test_suite()`, that returns
a pointer to a `FLTestSuite` describing your tests.

```c
/* my_tests.c */
#include <faultline/fl_test.h>
#include <faultline/fl_exception_service_assert.h>

/* FL_TEST defines a test function and a corresponding FLTestCase named <fn>_case. */
FL_TEST("one plus one equals two", test_addition) {
    FL_ASSERT_DETAILS(1 + 1 == 2, "expected 2");
}

FL_TEST("strlen of hello is 5", test_strlen) {
    FL_ASSERT_DETAILS(strlen("hello") == 5, "expected 5, got %zu", strlen("hello"));
}

/* Collect test cases into an array. */
FL_SUITE_BEGIN(my)
    FL_SUITE_ADD(test_addition)
    FL_SUITE_ADD(test_strlen)
FL_SUITE_END;

/* Define the suite struct and export fl_get_test_suite(). */
FL_GET_TEST_SUITE("My Suite", my)
```

### 4. Build and Run

**MSVC:**

```cmd
cl /DDLL_BUILD /I"C:\libs\FaultLine\include" my_tests.c /LD /OUT:my_tests.dll ^
   /link "C:\libs\FaultLine\lib\faultline.lib"

C:\libs\FaultLine\bin\faultline.exe run my_tests.dll
```

**CMake:**

```cmake
list(APPEND CMAKE_PREFIX_PATH "C:/libs/FaultLine")
find_package(Faultline CONFIG REQUIRED)

add_library(my_tests SHARED my_tests.c)
target_link_libraries(my_tests PRIVATE Faultline::faultline)
```

```cmd
C:\libs\FaultLine\bin\faultline.exe run my_tests.dll
```

## Fault Injection

FaultLine intercepts `malloc` and `free` calls inside your test suite DLL. For each
test case it runs a sequence of passes: on pass N it allows the first N−1 allocations
to succeed and fails the Nth. It repeats with increasing N until a complete pass
succeeds with no injected failure — meaning every allocation failure path has been
exercised.

## Database Integration

Pass `--db` to store results in a SQLite database and query them later:

```cmd
faultline.exe run --db results.sqlite my_tests.dll

faultline.exe show results  --db results.sqlite
faultline.exe show result 1 --db results.sqlite
faultline.exe show suites   --db results.sqlite
faultline.exe show hotspots --db results.sqlite
```

Run `faultline.exe help` or `faultline.exe help <command>` for the full list of options.

## Building with Clang / MSYS2

A Clang-based build is provided alongside the primary MSVC build. It generates a
`compile_commands.json` file as a side effect, which enables
[Include What You Use (IWYU)](https://include-what-you-use.org/) analysis.
See [docs/building-with-clang.md](docs/building-with-clang.md) for setup and usage.

## Project Layout

```
build/cmd/      MSVC build scripts
build/bash/     Clang/MSYS2 build scripts
build/ps/       PowerShell utilities (SDK install)
cmd/            Command-line executable sources
include/        Public SDK headers
src/            Library implementation and test suites
third_party/    External dependencies (SQLite, FNV hash, cwalk)
docs/           Additional documentation
target/         Build output (not in source control)
```

## License

MIT — see [LICENSE.txt](LICENSE.txt).

Third-party components in `third_party/` carry their own licenses:

| Component | License |
|-----------|---------|
| [SQLite](https://www.sqlite.org/) | Public domain |
| [FNV hash](http://www.isthe.com/chongo/tech/comp/fnv/) | Public domain |
| [cwalk](https://github.com/likle/cwalk) | MIT |
