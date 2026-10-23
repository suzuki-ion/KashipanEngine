param([string]$MSBuild = 'msbuild')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repo 'Build\JobHuntingLightTownRegression.generated.vcxproj'
$source = Get-Content -LiteralPath (Join-Path $repo 'Build\KashipanScriptChecker.vcxproj') -Raw
$source = $source.Replace('KashipanScriptChecker', 'JobHuntingLightTownRegression')
$source = $source.Replace('..\SourceFiles\JobHuntingLightTownRegression\main.cpp', '..\EngineTests\JobHuntingLightTownRegression.cpp')
$source = [regex]::Replace($source, '<ProjectReference[\s\S]*?</ProjectReference>', '')
$source = [regex]::Replace($source, '<PreBuildEvent>[\s\S]*?</PreBuildEvent>', '')
$libs = '$(OutDir)KashipanScriptRuntime.lib;$(OutDir)DirectXTex.lib;$(RepositoryRoot)Externals\angelscript\lib\angelscript64.lib;'
$source = $source.Replace('<AdditionalDependencies>', '<AdditionalDependencies>' + $libs)
try {
    Set-Content -LiteralPath $project -Value $source -Encoding utf8
    & $MSBuild $project /p:Configuration=Development /p:Platform=x64 "/p:SolutionDir=$repo\Build\" /m:1 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Regression build failed' }
    & (Join-Path $repo 'Generated\Outputs\x64\Development\JobHuntingLightTownRegression.generated.exe') $repo (Join-Path $repo 'Generated\LightTownRegression')
    if ($LASTEXITCODE -ne 0) { throw 'Regression failed' }
} finally {
    if (Test-Path -LiteralPath $project) { Remove-Item -LiteralPath $project }
}
