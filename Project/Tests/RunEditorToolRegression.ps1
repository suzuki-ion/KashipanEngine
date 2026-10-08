# Run from a Visual Studio x64 developer PowerShell. Uses editor configuration without starting the graphics engine.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskTestProject = Join-Path $taskProjectRoot 'EditorToolRegression.generated.vcxproj'
$taskSource = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'KashipanEngine.vcxproj') -Raw
$taskSource = $taskSource.Replace('KashipanEngine\KashipanEngine.cpp', 'Tests\EditorToolRegression.cpp')
$taskSource = $taskSource.Replace('<ClCompile Include="main.cpp" />', '')
$taskSource = $taskSource.Replace('<SubSystem>Windows</SubSystem>', '<SubSystem>Console</SubSystem>')
# Embedded debug info avoids dependency on the PDB server during this standalone check.
$taskSource = $taskSource.Replace('<ClCompile>', '<ClCompile><DebugInformationFormat>OldStyle</DebugInformationFormat>')
$taskSource = [regex]::Replace($taskSource, '<ProjectReference[\s\S]*?</ProjectReference>', '')
$taskSource = [regex]::Replace($taskSource, '<PostBuildEvent>[\s\S]*?</PostBuildEvent>', '')
$taskSource = [regex]::Replace($taskSource, '<PreBuildEvent>[\s\S]*?</PreBuildEvent>', '')
$taskLibraries = '$(ProjectDir)Externals\angelscript\lib\angelscript64.lib;$(OutDir)DirectXTex.lib;'
$taskSource = $taskSource.Replace('<AdditionalDependencies>', '<AdditionalDependencies>' + $taskLibraries)
try {
    Set-Content -LiteralPath $taskTestProject -Value $taskSource -Encoding utf8
    & msbuild $taskTestProject /p:Configuration=Development /p:Platform=x64 "/p:SolutionDir=$taskProjectRoot\" /m:1 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Editor tool regression build failed' }
    $taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated\Outputs\x64\Development'))
    & (Join-Path $taskOutputRoot 'EditorToolRegression.generated.exe') (Join-Path $taskProjectRoot 'EditorTools')
    if ($LASTEXITCODE -ne 0) { throw 'Editor tool regression cases failed' }
} finally {
    if (Test-Path -LiteralPath $taskTestProject) { Remove-Item -LiteralPath $taskTestProject }
}
