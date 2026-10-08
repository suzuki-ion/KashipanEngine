# Run after building the Development/x64 engine, in a Visual Studio x64 developer PowerShell.
# Reuses the exact production object files rather than compiling another copy of the engine.
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskObjectRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated\Obj\KashipanEngine\x64\Development'))
$taskTestProject = Join-Path $taskProjectRoot 'ShortTextBehaviorSceneRegression.generated.vcxproj'
$taskSource = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'KashipanEngine.vcxproj') -Raw
[xml]$taskXml = $taskSource
$taskNamespace = New-Object System.Xml.XmlNamespaceManager($taskXml.NameTable)
$taskNamespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$taskObjects = @()
foreach ($taskItem in $taskXml.SelectNodes('//m:ItemGroup/m:ClCompile[@Include]', $taskNamespace)) {
    $taskInclude = $taskItem.GetAttribute('Include')
    if ($taskInclude -eq 'main.cpp') { continue }
    $taskExcluded = $taskItem.SelectSingleNode("m:ExcludedFromBuild[contains(@Condition, 'Development') and contains(@Condition, 'x64')]", $taskNamespace)
    if ($taskExcluded -and $taskExcluded.InnerText -eq 'true') { continue }
    $taskObjectPath = Join-Path $taskObjectRoot ([IO.Path]::ChangeExtension($taskInclude, '.obj'))
    if (!(Test-Path -LiteralPath $taskObjectPath)) { throw "Build the Development engine first. Missing: $taskObjectPath" }
    $taskObjects += '<Object Include="' + [Security.SecurityElement]::Escape($taskObjectPath) + '" />'
}
$taskSource = [regex]::Replace($taskSource, '<ClCompile\b[^>]*Include="[^"]*"[^>]*(?:/>|>[\s\S]*?</ClCompile>)', '')
$taskSource = $taskSource.Replace('</Project>', '<ItemGroup><ClCompile Include="Tests\ShortTextBehaviorSceneRegression.cpp" />' + ($taskObjects -join "`n") + '</ItemGroup></Project>')
$taskSource = $taskSource.Replace('<SubSystem>Windows</SubSystem>', '<SubSystem>Console</SubSystem>')
$taskSource = $taskSource.Replace('<ClCompile>', '<ClCompile><DebugInformationFormat>OldStyle</DebugInformationFormat>')
$taskSource = [regex]::Replace($taskSource, '<ProjectReference[\s\S]*?</ProjectReference>', '')
$taskSource = [regex]::Replace($taskSource, '<(?:Pre|Post)BuildEvent>[\s\S]*?</(?:Pre|Post)BuildEvent>', '')
$taskLibraries = '$(ProjectDir)Externals\angelscript\lib\angelscript64.lib;$(OutDir)DirectXTex.lib;'
$taskSource = $taskSource.Replace('<AdditionalDependencies>', '<AdditionalDependencies>' + $taskLibraries)
try {
    Set-Content -LiteralPath $taskTestProject -Value $taskSource -Encoding utf8
    & msbuild $taskTestProject /p:Configuration=Development /p:Platform=x64 "/p:SolutionDir=$taskProjectRoot\" /m:1 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Short-text scene regression build failed' }
    $taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot '..\Generated\Outputs\x64\Development'))
    & (Join-Path $taskOutputRoot 'ShortTextBehaviorSceneRegression.generated.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Short-text scene regression failed' }
} finally {
    if (Test-Path -LiteralPath $taskTestProject) { Remove-Item -LiteralPath $taskTestProject }
}
