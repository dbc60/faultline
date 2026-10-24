@ECHO OFF

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: project.cmd - Repository-specific settings for the MSVC build. options.cmd, shell.cmd,
:: setup.cmd and config.cmd read these.
::
:: setup.cmd calls this before config.cmd so the settings are in setup.cmd's environment.
:: The DIR_* paths setup.cmd derives are already set.
SET DIR_FL=%DIR_REPO%

:: Include paths and compiler flags build_test_dll.cmd adds to config.cmd's flags
SET PROJECT_INCLUDES=/I%DIR_INCLUDE% /I%DIR_THIRD_PARTY% /I%DIR_REPO%\src
SET PROJECT_COMPILER_FLAGS=

:: The test driver that runs a suite, and the script under build\cmd\ that builds it
:: when it is missing. Leave the script empty for a prebuilt driver.
SET FL_TEST_DRIVER=%DIR_OUT_BIN%\faultline.exe
SET FL_TEST_DRIVER_SCRIPT=faultline.cmd
