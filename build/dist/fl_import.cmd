@ECHO OFF
SETLOCAL
:: See LICENSE.txt for copyright and licensing information about this file.
::
:: Thin wrapper around fl_import.ps1 for consumers who prefer a .cmd entry point.
:: All arguments are passed straight through. Uses Windows PowerShell (5.1+),
:: which is present on every supported Windows build.
::
:: Examples:
::   fl_import.cmd -From dist\memory -Into third_party\faultline
::   fl_import.cmd -Remove memory -Into third_party\faultline
::   fl_import.cmd -List -Into third_party\faultline

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fl_import.ps1" %*
EXIT /B %ERRORLEVEL%
