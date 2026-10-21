# Run in a Visual Studio x64 developer PowerShell after a Development build.
param([Parameter(Mandatory = $true)][string]$ProjectPath)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskProject = $taskRoot
$taskOutput = Join-Path $taskRoot 'Generated/StartupRegression'
$taskTarget = [IO.Path]::GetFullPath($ProjectPath)
if (-not (Test-Path -LiteralPath (Join-Path $taskTarget 'Assets') -PathType Container)) { throw 'ProjectPath must contain an Assets directory' }
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
& cl /nologo /std:c++20 /EHsc /W4 /utf-8 /FS /Z7 /DNOMINMAX "/I${taskProject}/SourceFiles/KashipanEngine" "/I${taskProject}/Externals/DirectXTex" "/I${taskProject}/Externals/nlohmann" "$PSScriptRoot/PrepareStartupTextures.cpp" "/Fo:$taskOutput/PrepareStartupTextures.obj" "/Fe:$taskOutput/PrepareStartupTextures.exe" /link "$taskRoot/Generated/Outputs/x64/Development/DirectXTex.lib" windowscodecs.lib ole32.lib d3d11.lib dxgi.lib
if ($LASTEXITCODE -ne 0) { throw 'Texture preparation tool build failed' }
& "$taskOutput/PrepareStartupTextures.exe" $taskTarget
if ($LASTEXITCODE -ne 0) { throw 'Texture preparation failed; see the per-file errors' }
