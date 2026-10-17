# Run from a Visual Studio x64 developer PowerShell. No network, model or graphics device.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
$taskDefaults = (Join-Path $taskProjectRoot 'EditorTools\ShortTextDefaults.json').Replace('\', '/')
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "/DSHORT_TEXT_DEFAULTS_PATH=\`"$taskDefaults\`"" "$PSScriptRoot\ShortTextEventRequestRegression.cpp" "$taskProjectRoot\KashipanEngine\Utilities\GameEvents.cpp" "$taskProjectRoot\KashipanEngine\Utilities\ShortTextEventRequest.cpp" "/Fo:$taskOutputRoot\\" "/Fe:$taskOutputRoot\ShortTextEventRequestRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text event grammar build failed' }
& "$taskOutputRoot\ShortTextEventRequestRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Short-text event grammar regression failed' }
