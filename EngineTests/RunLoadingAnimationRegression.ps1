$ErrorActionPreference = 'Stop'
$animationRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$animationOutput = Join-Path $animationRoot 'Generated/LoadingAnimation'
New-Item -ItemType Directory -Path $animationOutput -Force | Out-Null
Push-Location $animationRoot
try {
    $sources = @(
        "$PSScriptRoot/LoadingAnimationRegression.cpp",
        "$animationRoot/SourceFiles/KashipanEngine/Splash/LoadingAnimation.cpp",
        "$animationRoot/SourceFiles/KashipanEngine/Splash/LoadingAnimationRenderer.cpp",
        "$animationRoot/SourceFiles/KashipanEngine/Utilities/KeyframeAnimation.cpp",
        "$animationRoot/SourceFiles/KashipanEngine/Utilities/MathUtils/Easings.cpp",
        "$animationRoot/SourceFiles/KashipanEngine/Utilities/Conversion/ConvertString.cpp"
    )
    & cl /nologo /O1 /Gy /MT /DNOMINMAX /DUNICODE /D_UNICODE /std:c++20 /EHsc /utf-8 "/I$animationRoot/SourceFiles/KashipanEngine" "/I$animationRoot/SourceFiles/MyStd" "/I$animationRoot/Externals/nlohmann" "/I$animationRoot/Externals/utf8" @sources "/Fo:$animationOutput/" "/Fe:$animationOutput/Regression.exe" /link /OPT:REF user32.lib ole32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Loading animation regression build failed' }
    & "$animationOutput/Regression.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Loading animation regression failed' }
} finally { Pop-Location }
