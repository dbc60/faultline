@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone (fault-free) memory service package into
:: dist\memory_service\.
::
:: This package contains everything a new project's platform executable needs
:: to use the arena-only FaultLine memory service: the arena allocator,
:: platform- and application-side service implementations, and all public and
:: private headers required to compile them. It deliberately OMITS fault
:: injection (no fault_injector, no flp_fault_memory_service, no collections/FNV
:: that the injector needs). For the fault-injecting variant, use
:: fault_memory_service_dist.cmd instead.
::
:: Usage:
::   build\cmd\memory_service_dist.cmd [clean]
::
::   clean   - remove dist\memory_service\ entirely
::
:: Output layout:
::   dist\memory_service\src\      - C sources and private headers (compile these)
::   dist\memory_service\include\  - public headers (add to consumer include path)
::
:: Consumer build setup:
::   Include paths : dist\memory_service\include
::                   dist\memory_service\src       (private headers)
::   Compile       : dist\memory_service\src\*.c

SET PROJECT_NAME=Standalone Memory Service Distribution (no fault injection)
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\memory_service_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\memory_service
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline

:: Package metadata recorded in manifest.txt (bump SVC_VERSION on release).
:: This package bundles the exception and log service sources it needs, so it is
:: self-contained and declares no dependencies. fl_import reference-counts the
:: shared files, so importing it alongside the standalone exception/log packages
:: is still safe.
SET SVC_NAME=memory_service
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
MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: C source files — arena
:: -----------------------------------------------------------------------
ECHO Copying arena source files...
COPY /Y "%DIR_SRC%\arena.c"                    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_dbg.c"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_malloc.c"             "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_pool.c"               "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\digital_search_tree.c"      "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region.c"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_node.c"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_os.c"                "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_windows.c"           "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\lock_os.c"                  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\win32_lock.c"               "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\generic_lock.c"             "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: C source files — support services (exception + log + threads)
:: -----------------------------------------------------------------------
ECHO Copying support service source files...
COPY /Y "%DIR_SRC%\fl_exception_service.c"     "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_exception_service.c"    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_file_service.c"         "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_log_service.c"          "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fl_threads.c"               "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: C source files — memory service (both sides, arena-only)
:: -----------------------------------------------------------------------
ECHO Copying memory service source files...
COPY /Y "%DIR_SRC%\fla_memory_service.c"       "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\flp_memory_service.c"       "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Private headers — arena (needed to compile the sources; not public API)
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
COPY /Y "%DIR_SRC%\fl_lock.h"                  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\win32_platform.h"           "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers — memory service context
:: -----------------------------------------------------------------------
ECHO Copying memory service context headers...
COPY /Y "%DIR_INC%\flp_memory_context.h"            "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\ — arena + exception + log
:: -----------------------------------------------------------------------
ECHO Copying arena/exception/log public headers...
COPY /Y "%DIR_INC%\arena.h"                         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\arena_malloc.h"                  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\arena_pool.h"                    "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\dlist.h"                         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_abbreviated_types.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service.h"          "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_service_assert.h"   "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_exception_types.h"            "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_file_service.h"               "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_file_types.h"                 "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_log_service.h"                "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_macros.h"                     "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_threads.h"                    "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_try.h"                        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\size.h"                          "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\ — memory service
:: -----------------------------------------------------------------------
ECHO Copying memory service public headers...
COPY /Y "%DIR_INC%\fl_memory.h"                     "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_memory_service.h"             "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fla_memory_service.h"            "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\ (not include\faultline\)
:: -----------------------------------------------------------------------
ECHO Copying top-level public headers...
COPY /Y "%DIR_REPO%\include\flp_memory_service.h"   "%DIR_DIST%\include\" > NUL
COPY /Y "%DIR_REPO%\include\flp_exception_service.h" "%DIR_DIST%\include\" > NUL
COPY /Y "%DIR_REPO%\include\flp_file_service.h"     "%DIR_DIST%\include\" > NUL
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
ECHO Done. Package written to dist\memory_service\
ECHO.
ECHO   Consumer include paths:
ECHO     dist\memory_service\include   (public headers)
ECHO     dist\memory_service\src       (private headers)
ECHO   Compile these sources:
ECHO     dist\memory_service\src\*.c

GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
