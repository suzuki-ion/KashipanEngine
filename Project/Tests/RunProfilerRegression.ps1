# Run in a Visual Studio x64 developer PowerShell.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "$PSScriptRoot\ProfilerRegression.cpp" "/Fo:$taskOutputRoot\ProfilerRegression.obj" "/Fe:$taskOutputRoot\ProfilerRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler regression build failed' }
& "$taskOutputRoot\ProfilerRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler regression failed' }
