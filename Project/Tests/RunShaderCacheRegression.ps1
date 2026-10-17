# Run in a Visual Studio x64 developer PowerShell.
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskProject = Join-Path $taskRoot 'Project'
$taskOutput = Join-Path $taskRoot 'Generated/StartupRegression'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
Push-Location $taskRoot
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /FS /Z7 /DNOMINMAX "/I$taskProject/KashipanEngine" "/I$taskProject/Externals/nlohmann" "$PSScriptRoot/ShaderCacheRegression.cpp" "$taskProject/KashipanEngine/Graphics/Pipeline/System/ShaderCompiler.cpp" "$taskProject/KashipanEngine/Graphics/Pipeline/System/MaterialLayout.cpp" "$taskProject/KashipanEngine/Utilities/Conversion/ConvertString.cpp" "/Fo:$taskOutput\" "/Fe:$taskOutput/ShaderCacheRegression.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Shader cache regression compilation failed' }
    $taskPreviousPath = $env:PATH
    try {
        $env:PATH = "$taskRoot/Generated/Outputs/x64/Development;$env:PATH"
        & "$taskOutput/ShaderCacheRegression.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Shader cache regression failed' }
    } finally { $env:PATH = $taskPreviousPath }
} finally { Pop-Location }
