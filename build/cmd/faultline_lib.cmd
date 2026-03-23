@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

:: See LICENSE.txt for copyright and licensing information about this file.

SET PROJECT_NAME="FaultLine Library"
SET PROJECT_NAME=%PROJECT_NAME:"=%
TITLE %PROJECT_NAME%

SET DIR_CMD=%~dp0
SET DIR_CMD=%DIR_CMD:~0,-1%
CALL %DIR_CMD%\options.cmd %*

if %timed% EQU 1 (
    if NOT EXIST metrics\vs (
        md metrics\vs
    )
    ctime.exe -begin metrics\vs\faultline_lib.ctm
)

CALL %DIR_CMD%\setup.cmd %*

:: Build the project
IF %build% EQU 1 (
    if %verbose% EQU 1 (
        ECHO Build the %PROJECT_NAME%: %*
    )

    REM Clean stale obj files and lib
    if exist %DIR_OUT_OBJ%\arena.obj              del %DIR_OUT_OBJ%\arena.obj
    if exist %DIR_OUT_OBJ%\arena_dbg.obj          del %DIR_OUT_OBJ%\arena_dbg.obj
    if exist %DIR_OUT_OBJ%\arena_malloc.obj       del %DIR_OUT_OBJ%\arena_malloc.obj
    if exist %DIR_OUT_OBJ%\buffer.obj             del %DIR_OUT_OBJ%\buffer.obj
    if exist %DIR_OUT_OBJ%\digital_search_tree.obj del %DIR_OUT_OBJ%\digital_search_tree.obj
    if exist %DIR_OUT_OBJ%\fault_injector.obj     del %DIR_OUT_OBJ%\fault_injector.obj
    if exist %DIR_OUT_OBJ%\fla_memory_service.obj del %DIR_OUT_OBJ%\fla_memory_service.obj
    if exist %DIR_OUT_OBJ%\faultline_context.obj  del %DIR_OUT_OBJ%\faultline_context.obj
    if exist %DIR_OUT_OBJ%\faultline_driver.obj   del %DIR_OUT_OBJ%\faultline_driver.obj
    if exist %DIR_OUT_OBJ%\faultline_sqlite.obj   del %DIR_OUT_OBJ%\faultline_sqlite.obj
    if exist %DIR_OUT_OBJ%\fl_exception_service.obj del %DIR_OUT_OBJ%\fl_exception_service.obj
    if exist %DIR_OUT_OBJ%\fla_exception_service.obj del %DIR_OUT_OBJ%\fla_exception_service.obj
    if exist %DIR_OUT_OBJ%\fla_log_service.obj    del %DIR_OUT_OBJ%\fla_log_service.obj
    if exist %DIR_OUT_OBJ%\FNV64.obj              del %DIR_OUT_OBJ%\FNV64.obj
    if exist %DIR_OUT_OBJ%\region.obj             del %DIR_OUT_OBJ%\region.obj
    if exist %DIR_OUT_OBJ%\region_node.obj        del %DIR_OUT_OBJ%\region_node.obj
    if exist %DIR_OUT_OBJ%\set.obj                del %DIR_OUT_OBJ%\set.obj
    if exist %DIR_OUT_OBJ%\win_timer.obj          del %DIR_OUT_OBJ%\win_timer.obj
    if exist %DIR_OUT_OBJ%\sqlite3.obj            del %DIR_OUT_OBJ%\sqlite3.obj
    if exist %DIR_OUT_LIB%\faultline.lib          del %DIR_OUT_LIB%\faultline.lib

    REM Compile all library source files
    cl /c %CommonCompilerFlagsFinal% /wd4200 /wd4115 /DFL_EMBEDDED /I%DIR_INCLUDE% /I%DIR_REPO%\src /I%DIR_THIRD_PARTY% /I%DIR_THIRD_PARTY%\fnv ^
        %DIR_REPO%\src\arena.c ^
        %DIR_REPO%\src\arena_dbg.c ^
        %DIR_REPO%\src\arena_malloc.c ^
        %DIR_REPO%\src\buffer.c ^
        %DIR_REPO%\src\digital_search_tree.c ^
        %DIR_REPO%\src\fault_injector.c ^
        %DIR_REPO%\src\fla_memory_service.c ^
        %DIR_REPO%\src\faultline_context.c ^
        %DIR_REPO%\src\faultline_driver.c ^
        %DIR_REPO%\src\faultline_sqlite.c ^
        %DIR_REPO%\src\fl_exception_service.c ^
        %DIR_REPO%\src\fla_exception_service.c ^
        %DIR_REPO%\src\fla_log_service.c ^
        %DIR_THIRD_PARTY%\fnv\FNV64.c ^
        %DIR_REPO%\src\region.c ^
        %DIR_REPO%\src\region_node.c ^
        %DIR_REPO%\src\set.c ^
        %DIR_REPO%\src\win_timer.c ^
        %DIR_THIRD_PARTY%\sqlite\sqlite3.c ^
        /Fo:%DIR_OUT_OBJ%\ /Fd:%DIR_OUT_LIB%\faultline.pdb > "%TEMP%\cl_out.tmp"
    IF ERRORLEVEL 1 (
        type "%TEMP%\cl_out.tmp"
        del "%TEMP%\cl_out.tmp"
        echo Failed to compile %PROJECT_NAME%
        GOTO :ERROR
    )
    del "%TEMP%\cl_out.tmp"

    REM Create static library from .obj files
    lib %CommonLibrarianFlags% ^
        /OUT:%DIR_OUT_LIB%\faultline.lib ^
        %DIR_OUT_OBJ%\arena.obj ^
        %DIR_OUT_OBJ%\arena_dbg.obj ^
        %DIR_OUT_OBJ%\arena_malloc.obj ^
        %DIR_OUT_OBJ%\buffer.obj ^
        %DIR_OUT_OBJ%\digital_search_tree.obj ^
        %DIR_OUT_OBJ%\fault_injector.obj ^
        %DIR_OUT_OBJ%\fla_memory_service.obj ^
        %DIR_OUT_OBJ%\faultline_context.obj ^
        %DIR_OUT_OBJ%\faultline_driver.obj ^
        %DIR_OUT_OBJ%\faultline_sqlite.obj ^
        %DIR_OUT_OBJ%\fl_exception_service.obj ^
        %DIR_OUT_OBJ%\fla_exception_service.obj ^
        %DIR_OUT_OBJ%\fla_log_service.obj ^
        %DIR_OUT_OBJ%\FNV64.obj ^
        %DIR_OUT_OBJ%\region.obj ^
        %DIR_OUT_OBJ%\region_node.obj ^
        %DIR_OUT_OBJ%\set.obj ^
        %DIR_OUT_OBJ%\win_timer.obj ^
        %DIR_OUT_OBJ%\sqlite3.obj
    IF ERRORLEVEL 1 (
        echo Failed to create %PROJECT_NAME%
        GOTO :ERROR
    )

    if %verbose% EQU 1 (
        ECHO Successfully built %PROJECT_NAME%
    )
)
GOTO :SUCCESS

:ERROR
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_lib.ctm %errorlevel%
)
EXIT /B 1

:SUCCESS
if %timed% EQU 1 (
    ctime.exe -end metrics\vs\faultline_lib.ctm %errorlevel%
)
ENDLOCAL
EXIT /B 0
