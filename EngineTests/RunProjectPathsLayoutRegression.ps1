# Run from a Visual Studio x64 developer PowerShell. Uses isolated generated fixtures.
$ErrorActionPreference = 'Stop'
$layoutRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$layoutOutput = Join-Path $layoutRoot ('Generated\LayoutTests\' + [guid]::NewGuid().ToString('N'))
$layoutEngine = Join-Path $layoutRoot 'SourceFiles\KashipanEngine'
New-Item -ItemType Directory -Path $layoutOutput -Force | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /WX /utf-8 /FS "/I$layoutEngine" "/I$layoutRoot\Externals\nlohmann" "$PSScriptRoot\ProjectPathsLayoutRegression.cpp" "$layoutEngine\Core\ProjectPaths.cpp" "$layoutEngine\Utilities\Conversion\ConvertString.cpp" "/Fo$layoutOutput\\" "/Fe:$layoutOutput\Paths.exe" /link ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'ProjectPaths layout compilation failed' }
$layoutFixture = Join-Path $layoutOutput ('Fixture ' + [char]0x65e5 + [char]0x672c + [char]0x8a9e)
$layoutBin = Join-Path $layoutFixture 'Bin'
New-Item -ItemType Directory -Path $layoutBin -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $layoutOutput 'Paths.exe') -Destination $layoutBin
[IO.File]::WriteAllText((Join-Path $layoutBin 'EngineRoot.txt'), $layoutFixture, (New-Object Text.UTF8Encoding($false)))
foreach ($layoutGame in @('GameProjects\SampleGame', 'ExternalGame')) {
    $layoutGameRoot = Join-Path $layoutFixture $layoutGame
    New-Item -ItemType Directory -Path (Join-Path $layoutGameRoot 'Assets') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $layoutGameRoot 'Project.json'), '{}')
}
Push-Location $env:TEMP
try {
    & "$layoutBin\Paths.exe" --project SampleGame
    if ($LASTEXITCODE -ne 0) { throw 'Named game layout failed' }
    & "$layoutBin\Paths.exe" --project (Join-Path $layoutFixture 'ExternalGame')
    if ($LASTEXITCODE -ne 0) { throw 'Absolute game layout failed' }
    $layoutStandalone = Join-Path $layoutOutput 'Standalone'
    New-Item -ItemType Directory -Path (Join-Path $layoutStandalone 'Assets') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $layoutOutput 'Paths.exe') -Destination $layoutStandalone
    & "$layoutStandalone\Paths.exe" --standalone-check
    if ($LASTEXITCODE -ne 0) { throw 'Standalone layout failed' }
} finally { Pop-Location }
