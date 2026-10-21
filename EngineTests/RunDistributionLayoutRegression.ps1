# Validate real copy/distribution scripts against small isolated assets, without launching binaries.
$ErrorActionPreference = 'Stop'
$layoutRepository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$layoutFixture = Join-Path $layoutRepository ('Generated\DistributionLayoutTests\' + [guid]::NewGuid().ToString('N'))
$layoutOutput = Join-Path $layoutFixture 'Generated\Outputs\x64\Release'
function Write-LayoutFixture([string]$Relative, [string]$Content = 'fixture') {
    $layoutPath = Join-Path $layoutFixture $Relative
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($layoutPath)) -Force | Out-Null
    [IO.File]::WriteAllText($layoutPath, $Content)
}
function Assert-LayoutFile([string]$Relative) {
    if (!(Test-Path -LiteralPath (Join-Path $layoutFixture $Relative) -PathType Leaf)) { throw "Missing layout file: $Relative" }
}
foreach ($layoutFile in @('KashipanEngine.exe', 'dxcompiler.dll', 'dxil.dll', 'WebView2Loader.dll', 'LICENSE.txt')) {
    Write-LayoutFixture "Generated\Outputs\x64\Release\$layoutFile"
}
Write-LayoutFixture 'EngineData\Locales\en-US.json' '{}'
Write-LayoutFixture 'EngineData\UI\Splash\index.html'
Write-LayoutFixture 'EngineData\AssetsTemplate\Default\template.txt'
Write-LayoutFixture 'EngineData\EditorTools\Tool.as'
Write-LayoutFixture 'Docs\Reference\index.html'
Write-LayoutFixture 'GameProjects\Sample\Project.json' '{}'
Write-LayoutFixture 'GameProjects\Sample\Assets\payload.txt'
Write-LayoutFixture 'GameProjects\Sample\Assets\unused.txt'
Write-LayoutFixture 'EngineData\AssetsTemplate\Default\unused.txt'
Write-LayoutFixture 'EngineTools\DistributionAssetExclusions.Release.txt' 'unused.txt'
Write-LayoutFixture 'EngineTools\DistributionAssetExclusions.Development.txt' "GameProjects/Sample/Assets/unused.txt`nEngineData/AssetsTemplate/Default/unused.txt"
Write-LayoutFixture 'GameProjects\Sample\Logs\keep.txt'
Write-LayoutFixture 'UserSettings\Settings.json' '{"project.lastOpened":"Sample"}'
foreach ($layoutFile in @('profiler.py', 'protocol.py', 'single_instance.py', 'StartProfiler.cmd', 'README.md')) {
    Write-LayoutFixture "EngineTools\Profiler\$layoutFile"
}
New-Item -ItemType Directory -Path (Join-Path $layoutFixture 'EngineTools') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $layoutRepository 'EngineTools\AnalyzeReleaseAssets.py') -Destination (Join-Path $layoutFixture 'EngineTools')
& powershell -NoProfile -ExecutionPolicy Bypass -File "$layoutRepository\EngineTools\CopyReleaseAssets.ps1" -ProjectDir $layoutFixture -TargetDir $layoutOutput
if ($LASTEXITCODE -ne 0) { throw 'Release asset copy failed' }
Assert-LayoutFile 'Generated\Outputs\x64\Release\Assets\payload.txt'
Assert-LayoutFile 'Generated\Outputs\x64\Release\EngineData\Locales\en-US.json'
foreach ($layoutConfiguration in @('Release', 'Development')) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$layoutRepository\EngineTools\CreateDistribution.ps1" -ProjectDir $layoutFixture -TargetDir $layoutOutput -Platform x64 -Configuration $layoutConfiguration
    if ($LASTEXITCODE -ne 0) { throw "$layoutConfiguration packaging failed" }
}
Assert-LayoutFile 'Generated\Distribution\x64\Release\Assets\payload.txt'
Assert-LayoutFile 'Generated\Distribution\x64\Development\GameProjects\Sample\Project.json'
Assert-LayoutFile 'Generated\Distribution\x64\Development\GameProjects\Sample\Assets\payload.txt'
Assert-LayoutFile 'Generated\Distribution\x64\Development\Docs\Reference\index.html'
Assert-LayoutFile 'Generated\Distribution\x64\Development\EngineTools\Profiler\profiler.py'
Assert-LayoutFile 'GameProjects\Sample\Assets\unused.txt'
foreach ($layoutExcluded in @('Release\Assets\unused.txt', 'Development\GameProjects\Sample\Assets\unused.txt', 'Development\EngineData\AssetsTemplate\Default\unused.txt')) {
    if (Test-Path -LiteralPath (Join-Path $layoutFixture "Generated\Distribution\x64\$layoutExcluded")) { throw "Distribution exclusion failed: $layoutExcluded" }
}
if (Test-Path -LiteralPath (Join-Path $layoutFixture 'Generated\Distribution\x64\Development\GameProjects\Sample\Logs')) { throw 'Game logs entered distribution' }
if (Test-Path -LiteralPath (Join-Path $layoutFixture 'Generated\Distribution\x64\Release\EngineRoot.txt')) { throw 'Release contains an engine root marker' }
Write-LayoutFixture 'Generated\WorkCopy\x64\Release\stale.txt'
Write-LayoutFixture 'Generated\WorkCopy\x64\Release\Logs\keep.txt'
Write-LayoutFixture 'Generated\WorkCopy\x64\Release\UserSettings\Settings.json' '{}'
& powershell -NoProfile -ExecutionPolicy Bypass -File "$layoutRepository\EngineTools\CopyToWorkCopy.ps1" -ProjectDir $layoutFixture -TargetDir $layoutOutput -Platform x64 -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'WorkCopy sync failed' }
Assert-LayoutFile 'Generated\WorkCopy\x64\Release\Assets\payload.txt'
Assert-LayoutFile 'Generated\WorkCopy\x64\Release\Logs\keep.txt'
Assert-LayoutFile 'Generated\WorkCopy\x64\Release\UserSettings\Settings.json'
Assert-LayoutFile 'GameProjects\Sample\Logs\keep.txt'
if (Test-Path -LiteralPath (Join-Path $layoutFixture 'Generated\WorkCopy\x64\Release\stale.txt')) { throw 'WorkCopy retained a stale file' }
Write-Host 'Release/Development distribution and WorkCopy layouts passed (synthetic assets)'
