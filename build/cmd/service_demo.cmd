@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Build and run the platform/application service-split demo.
::
:: This is an end-to-end demonstration that the simplified package set composes:
::   1. produce the memory_service, stream_service, log_service, and exception_service
::      packages (log_service depends on stream_service for append/console output)
::   2. import all four into one unified tree (fl_import refcounts shared files)
::   3. build demo_suite.dll  (application side; the fla_ service shims)
::   4. build demo_driver.exe (platform side; the flp_ service implementations)
::   5. run the driver, which loads the DLL and injects the three application-facing
::      services (memory, log, exception); the demo never opens a log file by path,
::      so stream_service is only needed to satisfy flp_log_service.c's link-time
::      dependency, and the driver does not inject it into the DLL
::
:: The fla_ and flp_ exception sources cannot live in one binary (both define
:: fl_throw_assertion), so each executable compiles only its own side from the
:: shared tree. region_windows.c is unity-#included by region_os.c and is never
:: compiled as its own TU.
::
:: Usage:
::   build\cmd\service_demo.cmd [build options...]
::   build\cmd\service_demo.cmd clean
::
:: Build options (debug/release, x64/x86, vs2022, ...) are forwarded to
:: options.cmd / setup.cmd, matching the rest of the build system.

SET PROJECT_NAME=Service Split Demo (driver + DLL)
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%

:: setup.cmd exports DIR_REPO, CommonCompilerFlagsFinal, CommonLinkerFlagsFinal,
:: the VS PATH/INCLUDE/LIB, and the clean/cleanall flags.
CALL "%DIR_CMD%\options.cmd" %*
CALL "%DIR_CMD%\setup.cmd" %*

SET HERE=%DIR_REPO%\examples\service_demo
SET TREE=%DIR_REPO%\target\service_demo\tree
SET BIN=%DIR_REPO%\target\service_demo\bin
SET OBJ_ROOT=%DIR_REPO%\target\service_demo\obj
SET OBJ_DLL=%OBJ_ROOT%\dll
SET OBJ_EXE=%OBJ_ROOT%\exe
:: cl output capture, kept in the demo's own tree so it is removed by clean and
:: reclaimed by the next run instead of being shared with unrelated builds
SET "CL_LOG=%OBJ_ROOT%\service_demo_cl.tmp"

:: Honor clean/cleanall/cleanplat by removing the demo's private tree. The demo
:: keeps every artifact under target\service_demo (setup.cmd's clean targets the
:: standard target\ tree and never touches this one), so all three clean flavors
:: map to the same removal. When build is also requested, fall through and rebuild
:: after cleaning; mirroring the standard scripts where setup.cmd cleans first and
:: then the build proceeds.
SET _DO_CLEAN=0
IF %clean% EQU 1    SET _DO_CLEAN=1
IF %cleanall% EQU 1 SET _DO_CLEAN=1
IF %cleanplat% EQU 1 SET _DO_CLEAN=1

IF %_DO_CLEAN% EQU 1 (
    IF EXIST "%DIR_REPO%\target\service_demo" (
        ECHO Removing %DIR_REPO%\target\service_demo
        RD /S /Q "%DIR_REPO%\target\service_demo"
    )
)

:: A clean-only invocation (no build requested) is done now. setup.cmd leaves
:: build at 0 when a clean flag is given without an explicit build.
IF %build% NEQ 1 (
    ENDLOCAL
    EXIT /B 0
)

:: -----------------------------------------------------------------------
:: 1. Produce the four packages.
:: -----------------------------------------------------------------------
ECHO Producing packages...
CALL "%DIR_CMD%\memory_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist] memory_service FAILED & GOTO :FAIL )
CALL "%DIR_CMD%\stream_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist] stream_service FAILED & GOTO :FAIL )
CALL "%DIR_CMD%\log_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist] log_service FAILED & GOTO :FAIL )
CALL "%DIR_CMD%\exception_service_dist.cmd" > NUL
IF ERRORLEVEL 1 ( ECHO   [dist] exception_service FAILED & GOTO :FAIL )

:: -----------------------------------------------------------------------
:: 2. Import all four into one unified tree.
:: -----------------------------------------------------------------------
IF EXIST "%TREE%" RD /S /Q "%TREE%"
ECHO Importing packages into %TREE% ...
SET FL_IMPORT=%DIR_REPO%\build\dist\fl_import.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%FL_IMPORT%" -From "%DIR_REPO%\dist\memory_service" -Into "%TREE%"
IF ERRORLEVEL 1 ( ECHO   [import] memory_service FAILED & GOTO :FAIL )
powershell -NoProfile -ExecutionPolicy Bypass -File "%FL_IMPORT%" -From "%DIR_REPO%\dist\stream_service" -Into "%TREE%"
IF ERRORLEVEL 1 ( ECHO   [import] stream_service FAILED & GOTO :FAIL )
powershell -NoProfile -ExecutionPolicy Bypass -File "%FL_IMPORT%" -From "%DIR_REPO%\dist\log_service" -Into "%TREE%"
IF ERRORLEVEL 1 ( ECHO   [import] log_service FAILED & GOTO :FAIL )
powershell -NoProfile -ExecutionPolicy Bypass -File "%FL_IMPORT%" -From "%DIR_REPO%\dist\exception_service" -Into "%TREE%"
IF ERRORLEVEL 1 ( ECHO   [import] exception_service FAILED & GOTO :FAIL )

IF NOT EXIST "%BIN%"     MD "%BIN%"
IF NOT EXIST "%OBJ_DLL%" MD "%OBJ_DLL%"
IF NOT EXIST "%OBJ_EXE%" MD "%OBJ_EXE%"

:: Build sources as C code by default. If cxx is 1, then build sources as C++.
SET "DEMO_FLAGS=%CommonCompilerFlagsFinal%"
IF %cxx% EQU 1 SET "DEMO_FLAGS=%CommonCompilerFlagsFinalCXX%"

:: -----------------------------------------------------------------------
:: 3. Build the suite DLL (application side). DLL_BUILD makes FL_DECL_SPEC
::    dllexport so fla_set_* are exported; no FL_PLATFORM_BUILD, so the unified
::    headers select the fla_ shims. The DLL borrows the allocator/logger/
::    exception implementations from the driver at run time, so none are
::    compiled here.
:: -----------------------------------------------------------------------
ECHO Building demo_suite.dll ...
cl %DEMO_FLAGS% /DDLL_BUILD /wd4456 ^
    /I"%TREE%\include" /I"%TREE%\src" ^
    "%TREE%\src\fl_exception_service.c" ^
    "%TREE%\src\fla_exception_service.c" ^
    "%TREE%\src\fla_log_service.c" ^
    "%TREE%\src\fla_memory_service.c" ^
    "%HERE%\demo_suite.c" ^
    /Fo:"%OBJ_DLL%\\" /Fd:"%OBJ_DLL%\demo_suite.pdb" /LD /Fe:"%BIN%\demo_suite.dll" ^
    /link %CommonLinkerFlagsFinal% > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] demo_suite.dll FAILED & TYPE "%CL_LOG%" & GOTO :FAIL )

:: -----------------------------------------------------------------------
:: 4. Build the driver exe (platform side). FL_PLATFORM_BUILD selects the flp_
::    macros; FL_EMBEDDED makes FL_DECL_SPEC empty (single binary). Compile the
::    flp_/fl_ sources plus the arena; exclude region_windows.c (unity-included
::    by region_os.c) and every fla_ source (the DLL owns those).
:: -----------------------------------------------------------------------
ECHO Building demo_driver.exe ...
cl %DEMO_FLAGS% /DFL_PLATFORM_BUILD /DFL_EMBEDDED /wd4456 ^
    /I"%TREE%\include" /I"%TREE%\src" ^
    "%TREE%\src\fl_exception_service.c" ^
    "%TREE%\src\flp_exception_service.c" ^
    "%TREE%\src\flp_log_service.c" ^
    "%TREE%\src\fl_threads.c" ^
    "%TREE%\src\arena.c" ^
    "%TREE%\src\arena_dbg.c" ^
    "%TREE%\src\arena_malloc.c" ^
    "%TREE%\src\digital_search_tree.c" ^
    "%TREE%\src\region.c" ^
    "%TREE%\src\region_node.c" ^
    "%TREE%\src\region_os.c" ^
    "%TREE%\src\lock_os.c" ^
    "%TREE%\src\flp_memory_service.c" ^
    "%TREE%\src\flp_stream_service.c" ^
    "%HERE%\demo_driver.c" ^
    /Fo:"%OBJ_EXE%\\" /Fd:"%BIN%\demo_driver.pdb" /Fe:"%BIN%\demo_driver.exe" ^
    /link %CommonLinkerFlagsFinal% /ENTRY:mainCRTStartup > "%CL_LOG%" 2>&1
IF ERRORLEVEL 1 ( ECHO   [compile] demo_driver.exe FAILED & TYPE "%CL_LOG%" & GOTO :FAIL )

IF EXIST "%CL_LOG%" DEL "%CL_LOG%" 2> NUL

:: -----------------------------------------------------------------------
:: 5. Run the demo. The driver loads the DLL next to it and injects services.
:: -----------------------------------------------------------------------
ECHO.
ECHO Running demo (driver loads demo_suite.dll and injects all three services):
ECHO ------------------------------------------------------------
"%BIN%\demo_driver.exe" "%BIN%\demo_suite.dll"
SET RC=%ERRORLEVEL%
ECHO ------------------------------------------------------------
IF NOT "%RC%"=="0" ( ECHO Demo exited with code %RC%. & GOTO :FAIL )

ECHO.
ECHO Done. Built and ran:
ECHO   %BIN%\demo_suite.dll
ECHO   %BIN%\demo_driver.exe
GOTO :SUCCESS

:FAIL
ENDLOCAL
EXIT /B 1

:SUCCESS
ENDLOCAL
EXIT /B 0
