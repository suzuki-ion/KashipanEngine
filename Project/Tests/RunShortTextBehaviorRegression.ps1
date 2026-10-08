# Run from a Visual Studio x64 developer PowerShell. No network, model or graphics device.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "$PSScriptRoot\ShortTextBehaviorRegression.cpp" "/Fo:$taskOutputRoot\ShortTextBehaviorRegression.obj" "/Fe:$taskOutputRoot\ShortTextBehaviorRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text parser build failed' }
& "$taskOutputRoot\ShortTextBehaviorRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text parser regression failed' }
