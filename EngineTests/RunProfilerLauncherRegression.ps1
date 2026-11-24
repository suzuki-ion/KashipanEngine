# Visual Studio の x64 開発者用 PowerShell で、リポジトリのルートから実行する。
$ErrorActionPreference = 'Stop'
$profilerProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profilerOutput = [IO.Path]::GetFullPath((Join-Path $profilerProject 'Generated'))
New-Item -ItemType Directory -Path $profilerOutput -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I${profilerProject}\SourceFiles\KashipanEngine" "$PSScriptRoot\ProfilerLauncherRegression.cpp" "/Fo:$profilerOutput\ProfilerLauncherRegression.obj" "/Fe:$profilerOutput\ProfilerLauncherRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler launcher regression build failed' }
& "$profilerOutput\ProfilerLauncherRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler launcher regression failed' }
