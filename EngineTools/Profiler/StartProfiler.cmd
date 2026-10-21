@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
    python profiler.py %*
) else (
    py -3 profiler.py %*
)
if errorlevel 1 pause
