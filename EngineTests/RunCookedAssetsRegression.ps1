$ErrorActionPreference = 'Stop'
$cookRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cookOutput = Join-Path $cookRepo 'Generated/CookedRegression'
New-Item -ItemType Directory -Force -Path $cookOutput | Out-Null
$cookIncludes = @("/I$cookRepo/SourceFiles/KashipanEngine", "/I$cookRepo/Externals/nlohmann", "/I$cookRepo/Externals/DirectXTex", "/I$cookRepo/Externals/Assimp/include")
$cookSources = @("$PSScriptRoot/CookedAssetsRegression.cpp", "$cookRepo/SourceFiles/KashipanEngine/Graphics/Pipeline/System/ShaderCompiler.cpp", "$cookRepo/SourceFiles/KashipanEngine/Graphics/Pipeline/System/MaterialLayout.cpp", "$cookRepo/SourceFiles/KashipanEngine/Utilities/Conversion/ConvertString.cpp", "$cookRepo/SourceFiles/KashipanEngine/Utilities/FileIO/Directory.cpp")
$cookTextureLib = Join-Path $cookRepo 'Generated/Outputs/x64/Release/DirectXTex.lib'
if (-not (Test-Path -LiteralPath $cookTextureLib)) { throw 'Build Release DirectXTex first.' }
Push-Location $cookRepo
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /FS /Z7 /DNOMINMAX @cookIncludes @cookSources "/Fo:$cookOutput/" "/Fe:$cookOutput/CookedAssetsRegression.exe" /link $cookTextureLib "$cookRepo/Externals/Assimp/lib/Release/assimp-vc143-mt.lib" windowscodecs.lib ole32.lib d3d11.lib dxgi.lib
    if ($LASTEXITCODE -ne 0) { throw 'Cooked regression build failed' }
    $cookPreviousPath = $env:PATH
    try {
        $env:PATH = "$cookRepo/Generated/Outputs/x64/Release;$env:PATH"
        & "$cookOutput/CookedAssetsRegression.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Cooked regression failed' }
    } finally { $env:PATH = $cookPreviousPath }
} finally { Pop-Location }
