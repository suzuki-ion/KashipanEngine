# Run in a Visual Studio x64 developer PowerShell. Optional benchmark only reads Assets.
param([string]$BenchmarkProject = '')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutput = Join-Path $taskRoot 'Generated/AssetTreeOptimization-20261020'
New-Item -ItemType Directory -Force $taskOutput | Out-Null
Push-Location $taskRoot
try {
    & cl /nologo /std:c++20 /O2 /EHsc /W4 /WX /utf-8 /FS /Z7 "/I$taskRoot/SourceFiles/KashipanEngine" "$PSScriptRoot/AssetFolderTreeRegression.cpp" "$taskRoot/SourceFiles/KashipanEngine/Utilities/Conversion/ConvertString.cpp" "/Fo:$taskOutput\" "/Fe:$taskOutput/AssetFolderTreeRegression.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Folder tree regression compilation failed' }
    if ($BenchmarkProject) { & "$taskOutput/AssetFolderTreeRegression.exe" $BenchmarkProject }
    else { & "$taskOutput/AssetFolderTreeRegression.exe" }
    if ($LASTEXITCODE -ne 0) { throw 'Folder tree regression failed' }
} finally { Pop-Location }
