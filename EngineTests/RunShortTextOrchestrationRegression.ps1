$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot 'Generated'))
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\SourceFiles\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextOrchestrationRegression.cpp" "/Fo:$taskOutputRoot\ShortTextOrchestrationRegression.obj" "/Fe:$taskOutputRoot\ShortTextOrchestrationRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Orchestration regression build failed' }
& "$taskOutputRoot\ShortTextOrchestrationRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Orchestration regression failed' }
