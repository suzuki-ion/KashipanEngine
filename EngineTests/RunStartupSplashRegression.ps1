$ErrorActionPreference = 'Stop'
$splashRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$splashOutput = Join-Path $splashRoot 'Generated/StartupSplashRegression'
New-Item -ItemType Directory -Force -Path $splashOutput | Out-Null
Push-Location $splashRoot
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 "/I$splashRoot/SourceFiles/KashipanEngine" "$PSScriptRoot/SplashAnimationRegression.cpp" "/Fo:$splashOutput/Timeline.obj" "/Fe:$splashOutput/Timeline.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Timeline regression build failed' }
    & "$splashOutput/Timeline.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Timeline regression failed' }
    $splashDependencies = @(
        "$splashRoot/SourceFiles/KashipanEngine/Splash/LoadingAnimation.cpp",
        "$splashRoot/SourceFiles/KashipanEngine/Splash/LoadingAnimationRenderer.cpp",
        "$splashRoot/SourceFiles/KashipanEngine/Utilities/KeyframeAnimation.cpp",
        "$splashRoot/SourceFiles/KashipanEngine/Utilities/MathUtils/Easings.cpp",
        "$splashRoot/SourceFiles/KashipanEngine/Utilities/Conversion/ConvertString.cpp"
    )
    & cl /nologo /Gy /DNOMINMAX "/I$splashRoot/SourceFiles/MyStd" "/I$splashRoot/Externals/nlohmann" "/I$splashRoot/Externals/utf8" @splashDependencies /DRELEASE_BUILD /DUNICODE /D_UNICODE /std:c++20 /EHsc /W4 /utf-8 "/I$splashRoot/SourceFiles/KashipanEngine" "$PSScriptRoot/StartupSplashRenderingRegression.cpp" "/Fo:$splashOutput/" "/Fe:$splashOutput/Rendering.exe" /link /OPT:REF ole32.lib user32.lib gdi32.lib delayimp.lib /DELAYLOAD:d2d1.dll /DELAYLOAD:dwrite.dll
    if ($LASTEXITCODE -ne 0) { throw 'Rendering regression build failed' }
    & "$splashOutput/Rendering.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Rendering regression failed' }
    & cl /nologo /Gy /DNOMINMAX "/I$splashRoot/SourceFiles/MyStd" "/I$splashRoot/Externals/nlohmann" "/I$splashRoot/Externals/utf8" @splashDependencies /DRELEASE_BUILD /DUNICODE /D_UNICODE /std:c++20 /EHsc /W4 /utf-8 "/I$splashRoot/SourceFiles/KashipanEngine" "$PSScriptRoot/StartupSplashLifecycleRegression.cpp" "$splashRoot/SourceFiles/KashipanEngine/Splash/SplashScreen.cpp" "/Fo:$splashOutput/" "/Fe:$splashOutput/Lifecycle.exe" /link /OPT:REF ole32.lib /SUBSYSTEM:WINDOWS user32.lib gdi32.lib delayimp.lib /DELAYLOAD:d2d1.dll /DELAYLOAD:dwrite.dll
    if ($LASTEXITCODE -ne 0) { throw 'Lifecycle regression build failed' }
    foreach ($splashCase in @('early', 'late', 'cancel', 'missing', 'user-close', 'minimized', 'handoff', 'handoff-minimized', 'handoff-missing', 'abandon', 'custom', 'handoff-custom')) {
        $splashProcess = Start-Process -FilePath "$splashOutput/Lifecycle.exe" -ArgumentList $splashCase -WorkingDirectory $splashRoot -WindowStyle Hidden -PassThru
        if (-not $splashProcess.WaitForExit(15000)) {
            Stop-Process -Id $splashProcess.Id
            throw "Splash lifecycle timed out: $splashCase"
        }
        if ($splashProcess.ExitCode -ne 0) { throw "Splash lifecycle failed: $splashCase ($($splashProcess.ExitCode))" }
        Get-Content (Join-Path $splashOutput "$splashCase.log")
    }
    Write-Host 'Startup splash lifecycle regression passed (12 cases including custom and handoff-custom)'
} finally { Pop-Location }
