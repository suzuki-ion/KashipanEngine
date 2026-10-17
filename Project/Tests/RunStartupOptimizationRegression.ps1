# Run in a Visual Studio x64 developer PowerShell after a Development build.
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskProject = Join-Path $taskRoot 'Project'
$taskOutput = Join-Path $taskRoot 'Generated/StartupRegression'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
Push-Location $taskRoot
try {
    & "$PSScriptRoot/RunStartupTimingRegression.ps1"
    & "$PSScriptRoot/RunShaderCacheRegression.ps1"
    $taskIncludes = @("/I$taskProject/KashipanEngine", "/I$taskProject/Externals/nlohmann", "/I$taskProject/Externals/DirectXTex", "/I$taskProject/Externals/Assimp/include")
    foreach ($taskName in @('TextureUploadBatchRegression', 'PreparedTextureCacheRegression', 'SharedModelImportRegression', 'PersistentModelCacheRegression', 'AssetFileIndexRegression')) {
        $taskArguments = @('/nologo', '/std:c++20', '/EHsc', '/W4', '/utf-8', '/FS', '/Z7', '/DNOMINMAX') + $taskIncludes + @("$PSScriptRoot/$taskName.cpp", "/Fo:$taskOutput/", "/Fe:$taskOutput/$taskName.exe")
        if ($taskName -in @('SharedModelImportRegression', 'PersistentModelCacheRegression')) {
            $taskArguments += @("$taskProject/KashipanEngine/Utilities/Conversion/ConvertString.cpp", '/link', "/LIBPATH:$taskProject/Externals/Assimp/lib/Release", 'assimp-vc143-mt.lib')
        } elseif ($taskName -eq 'AssetFileIndexRegression') {
            $taskArguments += @("$taskProject/KashipanEngine/Utilities/FileIO/Directory.cpp", "$taskProject/KashipanEngine/Utilities/Conversion/ConvertString.cpp")
        } elseif ($taskName -eq 'PreparedTextureCacheRegression') {
            $taskArguments += @('/link', "$taskRoot/Generated/Outputs/x64/Development/DirectXTex.lib", 'windowscodecs.lib', 'ole32.lib', 'd3d11.lib', 'dxgi.lib')
        }
        & cl @taskArguments
        if ($LASTEXITCODE -ne 0) { throw "$taskName compilation failed" }
        $taskPreviousPath = $env:PATH
        try {
            $env:PATH = "$taskRoot/Generated/Outputs/x64/Development;$env:PATH"
            & "$taskOutput/$taskName.exe"
            if ($LASTEXITCODE -ne 0) { throw "$taskName failed" }
        } finally { $env:PATH = $taskPreviousPath }
    }
} finally { Pop-Location }
