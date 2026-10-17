# Run from a Visual Studio x64 developer PowerShell after building the same engine configuration.
# Examples: -SourceFile ShortTextRenderingRegression.cpp / ShortTextPhysicsRegression.cpp /
# ShortTextMediaRegression.cpp / ShortTextPropertiesRegression.cpp
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceFile,
    [ValidateSet('Development', 'Release')][string]$Configuration = 'Development',
    [string]$MSBuildPath = 'msbuild'
)
$ErrorActionPreference = 'Stop'
$taskProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskTestsRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$taskTestSourcePath = if ([IO.Path]::IsPathRooted($SourceFile)) { [IO.Path]::GetFullPath($SourceFile) } else { [IO.Path]::GetFullPath((Join-Path $taskTestsRoot $SourceFile)) }
if (![string]::Equals([IO.Path]::GetDirectoryName($taskTestSourcePath), $taskTestsRoot, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetExtension($taskTestSourcePath) -ne '.cpp' -or !(Test-Path -LiteralPath $taskTestSourcePath -PathType Leaf)) {
    throw 'SourceFile must identify an existing .cpp directly inside Project/Tests.'
}
$taskTestName = [IO.Path]::GetFileNameWithoutExtension($taskTestSourcePath) + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 12) + '.generated'
$taskTestProject = Join-Path $taskProjectRoot ($taskTestName + '.vcxproj')
$taskObjectRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot "..\Generated\Obj\KashipanEngine\x64\$Configuration"))
$taskOutputRoot = [IO.Path]::GetFullPath((Join-Path $taskProjectRoot "..\Generated\Outputs\x64\$Configuration"))
$taskExePath = Join-Path $taskOutputRoot ($taskTestName + '.exe')
$taskSource = Get-Content -LiteralPath (Join-Path $taskProjectRoot 'KashipanEngine.vcxproj') -Raw
[xml]$taskXml = $taskSource
$taskNamespace = New-Object System.Xml.XmlNamespaceManager($taskXml.NameTable)
$taskNamespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$taskObjects = @()
foreach ($taskItem in $taskXml.SelectNodes('//m:ItemGroup/m:ClCompile[@Include]', $taskNamespace)) {
    $taskInclude = $taskItem.GetAttribute('Include')
    if ($taskInclude -eq 'main.cpp') { continue }
    $taskExcluded = $taskItem.SelectSingleNode("m:ExcludedFromBuild[contains(@Condition, '$Configuration') and contains(@Condition, 'x64')]", $taskNamespace)
    if ($taskExcluded -and $taskExcluded.InnerText -eq 'true') { continue }
    $taskObjectPath = Join-Path $taskObjectRoot ([IO.Path]::ChangeExtension($taskInclude, '.obj'))
    if (!(Test-Path -LiteralPath $taskObjectPath -PathType Leaf)) { throw "Build the $Configuration engine first. Missing: $taskObjectPath" }
    $taskObjects += '<Object Include="' + [Security.SecurityElement]::Escape($taskObjectPath) + '" />'
}
$taskSource = [regex]::Replace($taskSource, '<ClCompile\b[^>]*Include="[^"]*"[^>]*(?:/>|>[\s\S]*?</ClCompile>)', '')
$taskRelativeTest = 'Tests\' + [IO.Path]::GetFileName($taskTestSourcePath)
$taskSource = $taskSource.Replace('</Project>', '<ItemGroup><ClCompile Include="' + [Security.SecurityElement]::Escape($taskRelativeTest) + '" />' + ($taskObjects -join "`n") + '</ItemGroup></Project>')
$taskSource = $taskSource.Replace('<SubSystem>Windows</SubSystem>', '<SubSystem>Console</SubSystem>')
$taskSource = $taskSource.Replace('<ClCompile>', '<ClCompile><DebugInformationFormat>OldStyle</DebugInformationFormat>')
$taskSource = [regex]::Replace($taskSource, '<ProjectReference[\s\S]*?</ProjectReference>', '')
$taskSource = [regex]::Replace($taskSource, '<(?:Pre|Post)BuildEvent>[\s\S]*?</(?:Pre|Post)BuildEvent>', '')
$taskLibraries = '$(ProjectDir)Externals\angelscript\lib\angelscript64.lib;$(OutDir)DirectXTex.lib;'
$taskSource = $taskSource.Replace('<AdditionalDependencies>', '<AdditionalDependencies>' + $taskLibraries)
try {
    Set-Content -LiteralPath $taskTestProject -Value $taskSource -Encoding utf8
    & $MSBuildPath $taskTestProject "/p:Configuration=$Configuration" /p:Platform=x64 "/p:SolutionDir=$taskProjectRoot\" /m:1 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "$SourceFile build/link failed" }
    & $taskExePath
    if ($LASTEXITCODE -ne 0) { throw "$SourceFile regression failed (exit $LASTEXITCODE)" }
} finally {
    # Only unique files created by this run are removed; production files and object files are retained.
    # The exe can stay briefly locked after exit (e.g. antivirus scan), so retry and only warn.
    foreach ($taskTransientPath in @($taskTestProject) + @('.exe', '.pdb', '.ilk', '.exp', '.lib' | ForEach-Object { Join-Path $taskOutputRoot ($taskTestName + $_) })) {
        for ($taskAttempt = 0; (Test-Path -LiteralPath $taskTransientPath) -and $taskAttempt -lt 20; ++$taskAttempt) {
            try { Remove-Item -LiteralPath $taskTransientPath -ErrorAction Stop } catch { Start-Sleep -Milliseconds 250 }
        }
        if (Test-Path -LiteralPath $taskTransientPath) { Write-Warning "Could not remove transient file: $taskTransientPath" }
    }
}
