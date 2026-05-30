@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone platform memory service package into dist\flp_memory_service\.
::
:: This package contains everything a new project's platform executable needs
:: to use the FaultLine memory service: the arena allocator, fault injector,
:: platform- and application-side service implementations, and all public and
:: private headers required to compile them.
::
:: Usage:
::   build\cmd\flp_memory_service_sa_dist.cmd [clean]
::
::   clean   - remove dist\flp_memory_service\ entirely
::
:: Output layout:
::   dist\flp_memory_service\src\      - C sources and private headers (compile these)
::   dist\flp_memory_service\src\fnv\  - FNV hash library (compile FNV64.c; add src\ to include path)
::   dist\flp_memory_service\include\  - public headers (add to consumer include path)
::
:: Consumer build setup:
::   Include paths : dist\flp_memory_service\include
::                   dist\flp_memory_service\src       (private headers + fnv\)
::   Compile       : dist\flp_memory_service\src\*.c
::                   dist\flp_memory_service\src\fnv\FNV64.c

SET PROJECT_NAME=Standalone Platform Memory Service Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\flp_memory_service_sa_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\flp_memory_service
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline
SET DIR_FNV=%DIR_REPO%\third_party\fnv

:: Package metadata recorded in manifest.txt (bump SVC_VERSION on release).
:: This package bundles the exception and log service sources it needs, so it is
:: self-contained and declares no dependencies. fl_import reference-counts the
:: shared files, so importing it alongside the standalone exception/log packages
:: is still safe.
SET SVC_NAME=memory
SET SVC_VERSION=0.2.0
SET SVC_DEPENDS=

:: Handle clean
IF /I "%~1"=="clean" (
    IF EXIST "%DIR_DIST%" (
        ECHO Removing %DIR_DIST%
        RD /S /Q "%DIR_DIST%"
    ) ELSE (
        ECHO Nothing to clean.
    )
    GOTO :SUCCESS
)

:: Wipe any previously-collected files so the package reflects exactly the
:: current file set and the generated manifest matches what is on disk.
IF EXIST "%DIR_DIST%\src"     RD /S /Q "%DIR_DIST%\src"
IF EXIST "%DIR_DIST%\include" RD /S /Q "%DIR_DIST%\include"
MD "%DIR_DIST%\src"
MD "%DIR_DIST%\src\fnv"
MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: C source files — arena
:: -----------------------------------------------------------------------
ECHO Copying arena source files...
COPY /Y "%DIR_SRC%\arena.c"                    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_dbg.c"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_malloc.c"             "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\buffer.c"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\digital_search_tree.c"      "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fl_exception_service.c"     "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fl_threads.c"               "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_exception_service.c"    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_log_service.c"          "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region.c"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_node.c"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_os.c"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_windows.c"           "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: C source files — collections
:: -----------------------------------------------------------------------
ECHO Copying collections source files...
COPY /Y "%DIR_SRC%\set.c"                      "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: C source files — FNV hash (placed in src\fnv\ to preserve the include path)
:: -----------------------------------------------------------------------
ECHO Copying FNV source files...
COPY /Y "%DIR_FNV%\FNV64.c"                    "%DIR_DIST%\src\fnv\" > NUL

:: -----------------------------------------------------------------------
:: C source files — fault injector
:: -----------------------------------------------------------------------
ECHO Copying fault injector source files...
COPY /Y "%DIR_SRC%\fault_injector.c"           "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: C source files — memory service (both sides)
:: -----------------------------------------------------------------------
ECHO Copying memory service source files...
COPY /Y "%DIR_SRC%\fla_memory_service.c"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_memory_service.c"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_fault_memory_service.c"      "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Private headers — arena
:: -----------------------------------------------------------------------
ECHO Copying arena private headers...
COPY /Y "%DIR_SRC%\arena_internal.h"           "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_dbg.h"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\atomic.h"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\bits.h"                     "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\chunk.h"                    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\digital_search_tree.h"      "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index.h"                    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index_generic.h"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index_intel.h"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index_linux.h"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index_windows.h"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region.h"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_node.h"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_os.h"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_windows.h"           "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\win32_platform.h"           "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Private headers — FNV hash
:: -----------------------------------------------------------------------
ECHO Copying FNV private headers...
COPY /Y "%DIR_FNV%\FNV64.h"                    "%DIR_DIST%\src\fnv\" > NUL
COPY /Y "%DIR_FNV%\FNVconfig.h"                "%DIR_DIST%\src\fnv\" > NUL
COPY /Y "%DIR_FNV%\FNVErrorCodes.h"            "%DIR_DIST%\src\fnv\" > NUL
COPY /Y "%DIR_FNV%\fnv-private.h"              "%DIR_DIST%\src\fnv\" > NUL

:: -----------------------------------------------------------------------
:: Private headers — fault injector
:: -----------------------------------------------------------------------
ECHO Copying fault injector private headers...
COPY /Y "%DIR_SRC%\fault_injector_internal.h"  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fault_injector_resource.h"  "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Private headers — memory service
:: -----------------------------------------------------------------------
ECHO Copying memory service private headers...
COPY /Y "%DIR_SRC%\flp_memory_context.h"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_fault_memory_context.h"      "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\ — arena + exception + log
:: -----------------------------------------------------------------------
ECHO Copying arena/exception/log public headers...
COPY /Y "%DIR_INC%\arena.h"                         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\arena_malloc.h"                  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\dlist.h"                         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_abbreviated_types.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service_assert.h"   "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_types.h"            "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log_types.h"                  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_macros.h"                     "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_threads.h"                    "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_try.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\size.h"                          "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\ — fault injector + memory service
:: -----------------------------------------------------------------------
ECHO Copying fault injector and memory service public headers...
COPY /Y "%DIR_INC%\buffer.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fault.h"                         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fault_injector.h"                "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fault_site.h"                    "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_memory_service.h"             "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fla_memory_service.h"            "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_result_codes.h"               "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_test_summary.h"               "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_types.h"                      "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\set.h"                           "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\timer.h"                         "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\ (not include\faultline\)
:: -----------------------------------------------------------------------
ECHO Copying top-level public headers...
COPY /Y "%DIR_REPO%\include\flp_memory_service.h"   "%DIR_DIST%\include\" > NUL
COPY /Y "%DIR_REPO%\include\flp_exception_service.h" "%DIR_DIST%\include\" > NUL
COPY /Y "%DIR_REPO%\include\flp_log_service.h"      "%DIR_DIST%\include\" > NUL

:: -----------------------------------------------------------------------
:: Generate the package manifest (authoritative file list used by fl_import)
:: -----------------------------------------------------------------------
ECHO Generating manifest...
powershell -NoProfile -ExecutionPolicy Bypass -File "%DIR_BUILD%\dist\fl_emit_manifest.ps1" -PackageDir "%DIR_DIST%" -Name "%SVC_NAME%" -Version "%SVC_VERSION%" -Depends "%SVC_DEPENDS%"
IF ERRORLEVEL 1 (
    ECHO ERROR: manifest generation failed.
    GOTO :FAIL
)

ECHO.
ECHO Done. Package written to dist\flp_memory_service\
ECHO.
ECHO   Consumer include paths:
ECHO     dist\flp_memory_service\include   (public headers)
ECHO     dist\flp_memory_service\src       (private headers; also covers src\fnv\)
ECHO   Compile these sources:
ECHO     dist\flp_memory_service\src\*.c
ECHO     dist\flp_memory_service\src\fnv\FNV64.c

GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
