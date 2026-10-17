# Run in Visual Studio x64 Developer PowerShell.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "/external:I$taskProjectRoot\Externals\nlohmann" /external:W0 "$PSScriptRoot\ProfilerServerHarness.cpp" "/Fo:$taskOutputRoot\ProfilerServerHarness.obj" "/Fe:$taskOutputRoot\ProfilerServerHarness.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler server harness build failed' }
& python "$taskProjectRoot\Tools\Profiler\test_profiler.py" --harness "$taskOutputRoot\ProfilerServerHarness.exe"
if ($LASTEXITCODE -ne 0) { throw 'Profiler integration regression failed' }
