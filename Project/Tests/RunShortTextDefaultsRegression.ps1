param([string]$CandidatePath)
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
if (!$CandidatePath) { $CandidatePath = Join-Path $taskProjectRoot 'EditorTools\ShortTextDefaults.json' }
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /DNOMINMAX "/I$taskProjectRoot\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextDefaultsRegression.cpp" "$taskProjectRoot\KashipanEngine\Utilities\ShortTextLearning.cpp" "/Fo:$taskOutputRoot\" "/Fe:$taskOutputRoot\ShortTextDefaultsRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Generated defaults regression build failed' }
& "$taskOutputRoot\ShortTextDefaultsRegression.exe" $CandidatePath
if ($LASTEXITCODE -ne 0) { throw 'Generated defaults validation failed' }
