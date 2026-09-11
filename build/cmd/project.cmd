@ECHO OFF

:: See LICENSE.txt for copyright and licensing information about this file.
::
:: project.cmd - Repository-specific settings for the MSVC build. options.cmd, shell.cmd,
:: setup.cmd and config.cmd read these.
::
:: setup.cmd calls this before config.cmd so the settings are in setup.cmd's environment.
SET DIR_FL=%DIR_REPO%
