# Run in a Visual Studio x64 developer PowerShell. Uses fake CLIs; no network or model credits.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\DevelopmentAssistantRegression.cpp" "$taskProjectRoot\KashipanEngine\Utilities\DevelopmentAssistant.cpp" "$taskProjectRoot\KashipanEngine\Utilities\DevelopmentAssistantTools.cpp" "/Fo$taskOutputRoot\\" "/Fe:$taskOutputRoot\DevelopmentAssistantRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant regression build failed' }
& "$taskOutputRoot\DevelopmentAssistantRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant regression failed' }
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "$PSScriptRoot\DevelopmentAssistantApiRegression.cpp" "$taskProjectRoot\KashipanEngine\Utilities\DevelopmentAssistantApiContext.cpp" "/Fo$taskOutputRoot\\" "/Fe:$taskOutputRoot\DevelopmentAssistantApiRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant API regression build failed' }
& "$taskOutputRoot\DevelopmentAssistantApiRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant API regression failed' }
