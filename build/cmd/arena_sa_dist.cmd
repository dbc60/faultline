@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Collect the standalone arena package into dist\arena\.
::
:: Usage:
::   build\cmd\arena_sa_dist.cmd [clean]
::
::   clean   - remove dist\arena\ entirely
::
:: Output layout:
::   dist\arena\src\      - C sources and private headers (compile these)
::   dist\arena\include\  - public headers (add to consumer include path)

SET PROJECT_NAME=Standalone Arena Distribution
TITLE %PROJECT_NAME%

:: Derive DIR_REPO from this script's location (build\cmd\arena_sa_dist.cmd)
SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_CMD%") DO SET DIR_BUILD=%%~dpF
SET DIR_BUILD=%DIR_BUILD:~0,-1%
FOR /f "delims=" %%F IN ("%DIR_BUILD%") DO SET DIR_REPO=%%~dpF
SET DIR_REPO=%DIR_REPO:~0,-1%

SET DIR_DIST=%DIR_REPO%\dist\arena
SET DIR_SRC=%DIR_REPO%\src
SET DIR_INC=%DIR_REPO%\include\faultline
SET DIR_SA=%DIR_REPO%\standalone\arena

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

:: Create output directories
IF NOT EXIST "%DIR_DIST%\src"                  MD "%DIR_DIST%\src"
IF NOT EXIST "%DIR_DIST%\include\faultline"    MD "%DIR_DIST%\include\faultline"

:: -----------------------------------------------------------------------
:: C source files
:: -----------------------------------------------------------------------
ECHO Copying source files...
COPY /Y "%DIR_SRC%\arena.c"                  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_dbg.c"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_malloc.c"           "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region.c"                 "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_node.c"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\digital_search_tree.c"    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\fl_threads.c"             "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SA%\src\fl_sa_exception_service.c" "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SA%\src\fl_sa_reasons.c"           "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Private headers (needed to compile the sources; not part of the public API)
:: -----------------------------------------------------------------------
ECHO Copying private headers...
COPY /Y "%DIR_SRC%\arena_internal.h"         "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\arena_dbg.h"              "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\atomic.h"                 "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\bits.h"                   "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\chunk.h"                  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\digital_search_tree.h"    "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\index.h"                  "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region.h"                 "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_node.h"            "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\region_windows.h"         "%DIR_DIST%\src\" > NUL
COPY /Y "%DIR_SRC%\win32_platform.h"         "%DIR_DIST%\src\" > NUL

:: -----------------------------------------------------------------------
:: Public headers from include\faultline\ (not overridden by standalone)
:: -----------------------------------------------------------------------
ECHO Copying public headers...
COPY /Y "%DIR_INC%\arena.h"                  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\arena_malloc.h"           "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\dlist.h"                  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_abbreviated_types.h"   "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_macros.h"              "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\fl_threads.h"             "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_INC%\size.h"                   "%DIR_DIST%\include\faultline\" > NUL

:: -----------------------------------------------------------------------
:: Standalone override headers (shadow the FaultLine exception machinery)
:: -----------------------------------------------------------------------
ECHO Copying standalone override headers...
COPY /Y "%DIR_SA%\include\faultline\fla_exception_service.h"        "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\faultline\fl_exception_service.h"         "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\faultline\fl_exception_service_assert.h"  "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\faultline\fl_exception_types.h"           "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\faultline\fl_log.h"                       "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\faultline\fl_try.h"                       "%DIR_DIST%\include\faultline\" > NUL
COPY /Y "%DIR_SA%\include\flp_exception_service.h"                  "%DIR_DIST%\include\" > NUL

ECHO.
ECHO Done. Package written to dist\arena\
ECHO.
ECHO   Consumer include path : dist\arena\include
ECHO   Compile these sources : dist\arena\src\*.c  (private headers are in src\ alongside them)

GOTO :SUCCESS

:SUCCESS
ENDLOCAL
EXIT /B 0
