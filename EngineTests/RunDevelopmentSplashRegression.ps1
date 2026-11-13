$ErrorActionPreference = 'Stop'
$legacyRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$legacyOutput = Join-Path $legacyRoot 'Generated/DevelopmentSplashRegression'
New-Item -ItemType Directory -Force -Path $legacyOutput | Out-Null
Push-Location $legacyRoot
try {
    & cl /nologo /DUNICODE /D_UNICODE /std:c++20 /EHsc /W4 /utf-8 /Z7 "/I$legacyRoot/SourceFiles/KashipanEngine" "/I$legacyRoot/Externals/nlohmann" "/I$legacyRoot/Externals/WebView2/include" "$PSScriptRoot/DevelopmentSplashRegression.cpp" "$legacyRoot/SourceFiles/KashipanEngine/Utilities/Conversion/ConvertString.cpp" "/Fo:$legacyOutput/" "/Fe:$legacyOutput/DevelopmentSplashRegression.exe" /link "/LIBPATH:$legacyRoot/Externals/WebView2/lib/x64" WebView2Loader.dll.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib
    if ($LASTEXITCODE -ne 0) { throw 'Development splash regression compilation failed' }
    Copy-Item -LiteralPath "$legacyRoot/Externals/WebView2/lib/x64/WebView2Loader.dll" -Destination $legacyOutput -Force
    foreach ($legacyCase in @('lifecycle','content','user-close','missing-ui')) {
        & "$legacyOutput/DevelopmentSplashRegression.exe" $legacyCase
        if ($LASTEXITCODE -ne 0) { throw "Development splash regression failed: $legacyCase" }
    }
} finally { Pop-Location }
