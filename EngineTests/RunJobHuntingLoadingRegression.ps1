# Development をビルドしてから、Visual Studio の x64 開発者用 PowerShell で実行する。
$ErrorActionPreference = 'Stop'
$bindingRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bindingOutput = Join-Path $bindingRoot 'Generated/JobHuntingLoading'
$bindingEngineOutput = Join-Path $bindingRoot 'Generated/Outputs/x64/Development'
$bindingLog = Join-Path $bindingRoot 'Generated/Obj/KashipanEngine/x64/Development/KashipanEngine.tlog/link.command.1.tlog'
if (-not (Test-Path -LiteralPath $bindingLog)) { throw 'Build Development first.' }
New-Item -ItemType Directory -Path $bindingOutput -Force | Out-Null
$bindingExe = Join-Path $bindingEngineOutput 'JobHuntingLoadingRegression.exe'
$bindingResponse = Join-Path $bindingOutput 'BindingLink.rsp'
$bindingArguments = ((Get-Content -LiteralPath $bindingLog -Encoding Unicode | Select-Object -Skip 1) -join "`n")
$bindingArguments = [regex]::Replace($bindingArguments, '/OUT:"[^"]*"', ('/OUT:"' + $bindingExe + '"'), 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '/SUBSYSTEM:WINDOWS', '/SUBSYSTEM:CONSOLE', 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '/(?:PDB|IMPLIB):"[^"]*"', '', 'IgnoreCase')
$bindingArguments = [regex]::Replace($bindingArguments, '[^\s"]*SOURCEFILES\\MAIN\.OBJ', '', 'IgnoreCase')
$bindingArguments += "`n`"$(Join-Path $bindingOutput 'JobHuntingLoadingRegression.obj')`"`n/DEBUG:NONE`n"
Set-Content -LiteralPath $bindingResponse -Value $bindingArguments -Encoding Unicode
$bindingIncludes = @('SourceFiles/KashipanEngine','SourceFiles/MyStd','SourceFiles/Application','Externals/ReactPhysics3D/include','Externals/Box2D/include','Externals/Assimp/include','Externals/imgui','Externals/DirectXTex','Externals/Common','Externals/utf8','Externals/nlohmann','Externals/angelscript/include') | ForEach-Object { '/I' + (Join-Path $bindingRoot $_) }
Push-Location $bindingRoot
try {
    & cl /nologo /c /O1 /Z7 /MT /DDEVELOPMENT_BUILD /DUSE_IMGUI /DNOMINMAX /DUNICODE /D_UNICODE /D_ITERATOR_DEBUG_LEVEL=0 /std:c++20 /EHsc /utf-8 /FI Debug/Logger.h @bindingIncludes "$PSScriptRoot/JobHuntingLoadingRegression.cpp" "/Fo:$bindingOutput/"
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression compile failed' }
    Push-Location (Join-Path $bindingRoot 'Build')
    try { & link "@$bindingResponse" } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression link failed' }
    & $bindingExe
    if ($LASTEXITCODE -ne 0) { throw 'Scene loading regression failed' }
} finally {
    Pop-Location
    foreach ($bindingExtension in @('exe','lib','exp','pdb','ilk')) {
        $bindingTemporary = Join-Path $bindingEngineOutput "JobHuntingLoadingRegression.$bindingExtension"
        for ($bindingAttempt=0; (Test-Path -LiteralPath $bindingTemporary) -and $bindingAttempt -lt 20; ++$bindingAttempt) {
            try { Remove-Item -LiteralPath $bindingTemporary -ErrorAction Stop } catch { Start-Sleep -Milliseconds 250 }
        }
        if (Test-Path -LiteralPath $bindingTemporary) { Write-Warning "Test passed, but the transient file is still locked: $bindingTemporary" }
    }
}
