@echo off
rem Auto-run entry point for win32k.sys profiling. Placed in the All Users
rem Startup folder so it runs without manual interaction inside the QEMU
rem guest. Results also go to the kernel debugger (ERR() via
rem WINE_DEFAULT_DEBUG_CHANNEL), readable from the host via the COM1 serial
rem port the LiveCD's debug boot entry redirects to.
win32kbench.exe all 3000
echo.
echo === win32kbench exit code: %ERRORLEVEL% ===
pause
