# Run from a Visual Studio x64 developer PowerShell. No network, model or graphics device.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot 'Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\SourceFiles\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextPropertyRequestRegression.cpp" "/Fo:$taskOutputRoot\ShortTextPropertyRequestRegression.obj" "/Fe:$taskOutputRoot\ShortTextPropertyRequestRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text property grammar build failed' }
& "$taskOutputRoot\ShortTextPropertyRequestRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text property grammar regression failed' }
