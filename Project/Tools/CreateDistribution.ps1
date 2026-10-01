[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$ProjectDir,
    [Parameter(Mandatory = $true)] [string]$TargetDir,
    [Parameter(Mandatory = $true)] [string]$Platform,
    [Parameter(Mandatory = $true)] [ValidateSet('Release', 'Development')] [string]$Configuration
)

$ErrorActionPreference = 'Stop'

function Remove-TrailingDot {
    param([string]$Path)
    return $Path.TrimEnd('.').TrimEnd('\')
}

function Invoke-MirrorCopy {
    param([string]$Source, [string]$Destination)
    robocopy $Source $Destination /MIR /NFL /NDL /NJH /NJS /NC /NS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed (exit code $LASTEXITCODE): $Source -> $Destination"
    }
}

try {
    if ($Platform -notmatch '^[A-Za-z0-9_-]+$') { throw "Invalid platform: $Platform" }

    $ProjectDir = [IO.Path]::GetFullPath((Remove-TrailingDot $ProjectDir))
    $TargetDir = [IO.Path]::GetFullPath((Remove-TrailingDot $TargetDir))
    $distributionBase = [IO.Path]::GetFullPath((Join-Path $ProjectDir '..\Generated\Distribution'))
    $platformDir = Join-Path $distributionBase $Platform
    $distributionDir = Join-Path $platformDir $Configuration
    $stageDir = Join-Path $platformDir ('.staging-' + [guid]::NewGuid().ToString('N'))
    $stagedReport = Join-Path $platformDir ('.asset-usage-' + [guid]::NewGuid().ToString('N') + '.json')

    if (-not (Test-Path -LiteralPath $TargetDir -PathType Container)) {
        throw "Build output directory was not found: $TargetDir"
    }
    if ([string]::Equals($TargetDir, $distributionDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Build output and distribution directory must be different.'
    }
    foreach ($path in @($distributionBase, $platformDir, $distributionDir)) {
        if ((Test-Path -LiteralPath $path) -and
            ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Distribution path must not be a reparse point: $path"
        }
    }

    $files = @('KashipanEngine.exe', 'dxcompiler.dll', 'dxil.dll', 'WebView2Loader.dll', 'LICENSE.txt')
    foreach ($name in $files) {
        if (-not (Test-Path -LiteralPath (Join-Path $TargetDir $name) -PathType Leaf)) {
            throw "Required distribution file was not found: $name"
        }
    }
    $assetSourceRoot = if ($Configuration -eq 'Release') { $TargetDir } else { $ProjectDir }
    foreach ($name in @('Locales', 'KashipanEngine\Splash\UI')) {
        if (-not (Test-Path -LiteralPath (Join-Path $assetSourceRoot $name) -PathType Container)) {
            throw "Required distribution directory was not found: $name"
        }
    }
    if ($Configuration -eq 'Release') {
        if (-not (Test-Path -LiteralPath (Join-Path $TargetDir 'Assets') -PathType Container)) {
            throw 'Required distribution directory was not found: Assets'
        }
    } else {
        foreach ($name in @('Projects', 'AssetsTemplate', 'EditorTools')) {
            if (-not (Test-Path -LiteralPath (Join-Path $ProjectDir $name) -PathType Container)) {
                throw "Required editor directory was not found: $name"
            }
        }
    }

    New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
    foreach ($name in $files) {
        Copy-Item -LiteralPath (Join-Path $TargetDir $name) -Destination (Join-Path $stageDir $name)
    }
    foreach ($name in @('Locales', 'KashipanEngine\Splash\UI')) {
        Invoke-MirrorCopy -Source (Join-Path $assetSourceRoot $name) -Destination (Join-Path $stageDir $name)
    }
    if ($Configuration -eq 'Release') {
        Invoke-MirrorCopy -Source (Join-Path $TargetDir 'Assets') -Destination (Join-Path $stageDir 'Assets')
    } else {
        # Keep the normal multi-project editor layout. Assets beside the exe would select standalone mode.
        foreach ($name in @('AssetsTemplate', 'EditorTools')) {
            Invoke-MirrorCopy -Source (Join-Path $ProjectDir $name) -Destination (Join-Path $stageDir $name)
        }
        $projectsDestination = Join-Path $stageDir 'Projects'
        New-Item -ItemType Directory -Path $projectsDestination -Force | Out-Null
        foreach ($project in (Get-ChildItem -LiteralPath (Join-Path $ProjectDir 'Projects') -Directory)) {
            $projectFile = Join-Path $project.FullName 'Project.json'
            if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) { continue }
            $projectAssets = Join-Path $project.FullName 'Assets'
            if (-not (Test-Path -LiteralPath $projectAssets -PathType Container)) {
                throw "Project Assets directory was not found: $projectAssets"
            }
            $projectDestination = Join-Path $projectsDestination $project.Name
            New-Item -ItemType Directory -Path $projectDestination -Force | Out-Null
            Copy-Item -LiteralPath $projectFile -Destination (Join-Path $projectDestination 'Project.json')
            Invoke-MirrorCopy -Source $projectAssets -Destination (Join-Path $projectDestination 'Assets')
        }
    }

    # One path per line, relative to Assets/. Lines beginning with # are comments.
    $exclusionFile = Join-Path $ProjectDir ("Tools\DistributionAssetExclusions.$Configuration.txt")
    $exclusionRoot = [IO.Path]::GetFullPath((Join-Path $stageDir $(if ($Configuration -eq 'Release') { 'Assets' } else { '.' })))
    if (Test-Path -LiteralPath $exclusionFile -PathType Leaf) {
        foreach ($line in (Get-Content -LiteralPath $exclusionFile -Encoding UTF8)) {
            $relativePath = $line.Trim().TrimEnd('\', '/')
            if (-not $relativePath -or $relativePath.StartsWith('#')) { continue }
            $excludedPath = [IO.Path]::GetFullPath((Join-Path $exclusionRoot $relativePath))
            if (-not $excludedPath.StartsWith($exclusionRoot + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw "Asset exclusion is outside the distribution assets: $relativePath"
            }
            if ($Configuration -eq 'Development') {
                $normalized = $relativePath.Replace('/', '\')
                if ($normalized -notmatch '^(?i:Projects\\[^\\]+\\Assets\\.+|AssetsTemplate\\.+)$') {
                    throw "Development exclusions must be under a project Assets/ or AssetsTemplate/: $relativePath"
                }
            }
            $pathToCheck = $excludedPath
            while ($pathToCheck.StartsWith($exclusionRoot + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase)) {
                if ((Test-Path -LiteralPath $pathToCheck) -and
                    ((Get-Item -LiteralPath $pathToCheck -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                    throw "Asset exclusion must not traverse a reparse point: $relativePath"
                }
                $pathToCheck = [IO.Path]::GetDirectoryName($pathToCheck)
            }
            if (Test-Path -LiteralPath $excludedPath) {
                Remove-Item -LiteralPath $excludedPath -Recurse -Force
            }
        }
    }

    if ($Configuration -eq 'Release') {
        $analyzer = Join-Path $ProjectDir 'Tools\AnalyzeReleaseAssets.py'
        if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf)) {
            throw "Release asset analyzer was not found: $analyzer"
        }
        if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
            throw 'Python 3 is required for Release asset analysis.'
        }
        & python $analyzer --assets-root (Join-Path $stageDir 'Assets') --report $stagedReport --apply
        if ($LASTEXITCODE -ne 0) {
            throw "Release asset analysis failed (exit code $LASTEXITCODE)"
        }
    }

    Write-Host "CreateDistribution: '$stageDir' -> '$distributionDir'"
    Invoke-MirrorCopy -Source $stageDir -Destination $distributionDir
    if ($Configuration -eq 'Release') {
        Move-Item -LiteralPath $stagedReport -Destination (Join-Path $platformDir 'Release.asset-usage.json') -Force
    }
    Write-Host "Distribution ready: $distributionDir"
    exit 0
} catch {
    Write-Error $_
    exit 1
} finally {
    if ($stageDir -and (Test-Path -LiteralPath $stageDir -PathType Container)) {
        Remove-Item -LiteralPath $stageDir -Recurse -Force
    }
    if ($stagedReport -and (Test-Path -LiteralPath $stagedReport -PathType Leaf)) {
        Remove-Item -LiteralPath $stagedReport -Force
    }
}
