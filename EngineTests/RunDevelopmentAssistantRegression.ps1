# Visual Studio の x64 開発者用 PowerShell で実行する。疑似 CLI を使うため、ネットワークやモデルの利用枠は不要。
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot 'Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\SourceFiles\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\DevelopmentAssistantRegression.cpp" "$taskProjectRoot\SourceFiles\KashipanEngine\Utilities\DevelopmentAssistant.cpp" "$taskProjectRoot\SourceFiles\KashipanEngine\Utilities\DevelopmentAssistantTools.cpp" "/Fo$taskOutputRoot\\" "/Fe:$taskOutputRoot\DevelopmentAssistantRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant regression build failed' }
& "$taskOutputRoot\DevelopmentAssistantRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant regression failed' }
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\SourceFiles\KashipanEngine" "$PSScriptRoot\DevelopmentAssistantApiRegression.cpp" "$taskProjectRoot\SourceFiles\KashipanEngine\Utilities\DevelopmentAssistantApiContext.cpp" "/Fo$taskOutputRoot\\" "/Fe:$taskOutputRoot\DevelopmentAssistantApiRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant API regression build failed' }
& "$taskOutputRoot\DevelopmentAssistantApiRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Development assistant API regression failed' }
