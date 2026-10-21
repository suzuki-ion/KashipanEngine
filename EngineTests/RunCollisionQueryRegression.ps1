# Run in a Visual Studio x64 developer PowerShell, after building the Development
# KashipanScriptChecker target (which produces the required runtime libraries).
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testProject = Join-Path $projectRoot 'Build\CollisionQueryRegression.generated.vcxproj'
$source = Get-Content -LiteralPath (Join-Path $projectRoot 'Build\KashipanScriptChecker.vcxproj') -Raw
$source = $source.Replace('KashipanScriptChecker', 'CollisionQueryRegression')
$source = $source.Replace('..\SourceFiles\CollisionQueryRegression\main.cpp', '..\EngineTests\CollisionQueryRegression.cpp')
$source = [regex]::Replace($source, '<ProjectReference[\s\S]*?</ProjectReference>', '')
$libraries = '$(OutDir)KashipanScriptRuntime.lib;$(OutDir)DirectXTex.lib;$(RepositoryRoot)Externals\angelscript\lib\angelscript64.lib;'
$source = $source.Replace('<AdditionalDependencies>', '<AdditionalDependencies>' + $libraries)
try {
    Set-Content -LiteralPath $testProject -Value $source -Encoding utf8
    & msbuild $testProject /p:Configuration=Development /p:Platform=x64 "/p:SolutionDir=$projectRoot\Build\" /m:1 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Regression build failed' }
    $outputRoot = Join-Path $projectRoot 'Generated\Outputs\x64\Development'
    & (Join-Path $outputRoot 'CollisionQueryRegression.generated.exe') (Join-Path $outputRoot 'CollisionQuery.as.predefined') (Join-Path $PSScriptRoot 'CollisionQueryExample.as')
    if ($LASTEXITCODE -ne 0) { throw 'Regression cases failed' }
} finally {
    # This is a single generated project file, never the checkout or a directory.
    if (Test-Path -LiteralPath $testProject) { Remove-Item -LiteralPath $testProject }
}
