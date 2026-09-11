@echo off
setlocal

set "CONFIGURATION=%~1"
if not defined CONFIGURATION set "CONFIGURATION=Release"

set "PLATFORM=%~2"
if not defined PLATFORM set "PLATFORM=x64"

if /I not "%CONFIGURATION%"=="Debug" if /I not "%CONFIGURATION%"=="Release" if /I not "%CONFIGURATION%"=="Development" (
    echo Usage: %~nx0 [Debug^|Release^|Development] [Platform]
    exit /b 2
)

for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
for %%I in ("%PROJECT_DIR%\..\Generated\Outputs\%PLATFORM%\%CONFIGURATION%") do set "TARGET_DIR=%%~fI"

if not exist "%TARGET_DIR%" (
    echo Build output directory was not found: "%TARGET_DIR%"
    exit /b 1
)

if /I "%CONFIGURATION%"=="Release" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\Tools\CopyReleaseAssets.ps1" -ProjectDir "%PROJECT_DIR%" -TargetDir "%TARGET_DIR%"
    if errorlevel 1 exit /b %errorlevel%
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\Tools\CopyToWorkCopy.ps1" -ProjectDir "%PROJECT_DIR%" -TargetDir "%TARGET_DIR%" -Platform "%PLATFORM%" -Configuration "%CONFIGURATION%"
if errorlevel 1 exit /b %errorlevel%

echo Asset copy completed: %PLATFORM%\%CONFIGURATION%
exit /b 0
