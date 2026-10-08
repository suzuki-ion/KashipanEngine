# Run in a Visual Studio x64 developer PowerShell. Tests the production dictionary without starting the editor.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /DNOMINMAX "/I$taskProjectRoot\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextLearningRegression.cpp" "$taskProjectRoot\KashipanEngine\Utilities\ShortTextLearning.cpp" "/Fo:$taskOutputRoot\" "/Fe:$taskOutputRoot\ShortTextLearningRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Local learning regression build failed' }
& "$taskOutputRoot\ShortTextLearningRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Local learning regression failed' }
