$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated'))
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 "/I$taskProjectRoot\KashipanEngine" "/I$taskProjectRoot\Externals\nlohmann" "$PSScriptRoot\ShortTextProgramRegression.cpp" "/Fo:$taskOutputRoot\ShortTextProgramRegression.obj" "/Fe:$taskOutputRoot\ShortTextProgramRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Program regression build failed' }
& "$taskOutputRoot\ShortTextProgramRegression.exe"
if ($LASTEXITCODE -ne 0) { throw 'Program regression failed' }
