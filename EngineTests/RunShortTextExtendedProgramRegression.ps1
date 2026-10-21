$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot 'Generated'))
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\SourceFiles\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextExtendedProgramRegression.cpp" "/Fo:$taskOutputRoot\ShortTextExtendedProgramRegression.obj" "/Fe:$taskOutputRoot\ShortTextExtendedProgramRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Extended program regression build failed' }
& "$taskOutputRoot\ShortTextExtendedProgramRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Extended program regression failed' }
