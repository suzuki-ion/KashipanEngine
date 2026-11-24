# Visual Studio の x64 開発者用 PowerShell で実行する。エディターを起動せず、実際の辞書をテストする。
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot 'Generated'))
New-Item -ItemType Directory -Path $taskOutputRoot -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /DNOMINMAX "/I$taskProjectRoot\SourceFiles\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextLearningRegression.cpp" "$taskProjectRoot\SourceFiles\KashipanEngine\Utilities\ShortTextLearning.cpp" "/Fo:$taskOutputRoot\" "/Fe:$taskOutputRoot\ShortTextLearningRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Local learning regression build failed' }
& "$taskOutputRoot\ShortTextLearningRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Local learning regression failed' }
