@echo off
rem Auto-run entry point for win32k.sys M4 profiling (see win32k-audit-plan
rem memory). Placed in the All Users Startup folder so it runs without any
rem manual interaction inside the QEMU guest; results go to stdout AND the
rem kernel debugger (OutputDebugStringA), readable from the host via the
rem COM1 serial port the LiveCD's debug boot entry already redirects to.
win32kbench.exe all 3000 > %SystemDrive%\win32kbench-results.txt 2>&1
shutdown.exe -s -t 5 -f
