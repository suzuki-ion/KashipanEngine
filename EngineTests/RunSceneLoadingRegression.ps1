param([ValidateSet("Development","Release")][string]$Configuration="Development")
# Run in a VS x64 Developer PowerShell after building the selected configuration.
$ErrorActionPreference = 'Stop'
$bindingRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bindingOutput = Join-Path $bindingRoot 'Generated/SceneLoading'
$bindingEngineOutput = Join-Path $bindingRoot "Generated/Outputs/x64/$Configuration"
$bindingLog = Join-Path $bindingRoot "Generated/Obj/KashipanEngine/x64/$Configuration/KashipanEngine.tlog/link.command.1.tlog"
if (-not (Test-Path -LiteralPath $bindingLog)) { throw "Build $Configuration first." }
New-Item -ItemType Directory -Path $bindingOutput -Force | Out-Null
$bindingExe = Join-Path $bindingEngineOutput 'SceneLoadingRegression.exe'
$bindingResponse = Join-Path $bindingOutput 'BindingLink.rsp'
$bindingArguments = ((Get-Content -LiteralPath $bindingLog -Encoding Unicode | Select-Object -Skip 1) -join "`n")
$bindingArguments = [regex]::Replace($bindingArguments, '/OUT:"[^"]*"', ('/OUT:"' + $bindingExe + '"'), 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '/SUBSYSTEM:WINDOWS', '/SUBSYSTEM:CONSOLE', 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '/(?:PDB|IMPLIB):"[^"]*"', '', 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '/LTCGOUT:"[^"]*"', ('/LTCGOUT:"' + (Join-Path $bindingOutput "SceneLoadingRegression.$Configuration.iobj") + '"'), 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '[^\s"]*SOURCEFILES\\MAIN\.OBJ', '', 'IgnoreCase')
$bindingArguments += "`n`"$(Join-Path $bindingOutput 'SceneLoadingRegression.obj')`"`n/DEBUG:NONE`n"
Set-Content -LiteralPath $bindingResponse -Value $bindingArguments -Encoding Unicode
$bindingIncludes = @('SourceFiles/KashipanEngine','SourceFiles/MyStd','SourceFiles/Application','Externals/ReactPhysics3D/include','Externals/Box2D/include','Externals/Assimp/include','Externals/imgui','Externals/DirectXTex','Externals/Common','Externals/utf8','Externals/nlohmann','Externals/angelscript/include') | ForEach-Object { '/I' + (Join-Path $bindingRoot $_) }
Push-Location $bindingRoot
try {
    $configurationFlags = if ($Configuration -eq "Release") { @("/DRELEASE_BUILD","/DNDEBUG") } else { @("/DDEVELOPMENT_BUILD","/DUSE_IMGUI") }
    & cl /nologo /c /O1 /Z7 /MT @configurationFlags /DNOMINMAX /DUNICODE /D_UNICODE /D_ITERATOR_DEBUG_LEVEL=0 /std:c++20 /EHsc /utf-8 /FI Debug/Logger.h @bindingIncludes "$PSScriptRoot/SceneLoadingRegression.cpp" "/Fo:$bindingOutput/"
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression compile failed' }
    Push-Location (Join-Path $bindingRoot 'Build')
    try { & link "@$bindingResponse" } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression link failed' }
    & $bindingExe
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression failed' }
} finally {
    Pop-Location
    foreach ($bindingExtension in @('exe','lib','exp','pdb','ilk')) {
        $bindingTemporary = Join-Path $bindingEngineOutput "SceneLoadingRegression.$bindingExtension"
        for ($bindingAttempt=0; (Test-Path -LiteralPath $bindingTemporary) -and $bindingAttempt -lt 20; ++$bindingAttempt) {
            try { Remove-Item -LiteralPath $bindingTemporary -ErrorAction Stop } catch { Start-Sleep -Milliseconds 250 }
        }
        if (Test-Path -LiteralPath $bindingTemporary) { Write-Warning "Test passed, but the transient file is still locked: $bindingTemporary" }
    }
}
