# Run in a Visual Studio x64 developer PowerShell. Requires a D3D12-capable adapter.
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskProject = $taskRoot
$taskOutput = Join-Path $taskRoot 'Generated/StartupRegression'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
Push-Location $taskRoot
try {
    & cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /FS /Z7 /DNOMINMAX "/I${taskProject}/SourceFiles/KashipanEngine" "/I${taskProject}/Externals/nlohmann" "$PSScriptRoot/PipelineStateCacheRegression.cpp" "/Fo:$taskOutput\" "/Fe:$taskOutput/PipelineStateCacheRegression.exe"
    if ($LASTEXITCODE -ne 0) { throw 'PSO cache regression compilation failed' }
    & "$taskOutput/PipelineStateCacheRegression.exe"
    if ($LASTEXITCODE -ne 0) { throw 'PSO cache regression failed' }
} finally { Pop-Location }
