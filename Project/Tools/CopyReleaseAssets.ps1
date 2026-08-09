[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [Parameter(Mandatory = $true)]
    [string]$TargetDir
)

$ErrorActionPreference = "Stop"

# $(ProjectDir) / $(TargetDir) は末尾が "\" のため、末尾に "." を付けて渡されている。
# ここで取り除いて実際のフォルダパスに戻す。
function Remove-TrailingDot {
    param([string]$Path)
    return $Path.TrimEnd('.').TrimEnd('\')
}

$ProjectDir = Remove-TrailingDot $ProjectDir
$TargetDir = Remove-TrailingDot $TargetDir

$userSettingsPath = Join-Path $ProjectDir "UserSettings\Settings.json"
$legacyUserSettingsPath = Join-Path $ProjectDir "UserSettings.json"
$projectsRoot = Join-Path $ProjectDir "Projects"

function Get-LastOpenedProjectName {
    foreach ($path in @($userSettingsPath, $legacyUserSettingsPath)) {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $json = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
        $name = $json."project.lastOpened"
        if ($name) { return $name }
    }
    return $null
}

# /MIR で差分のみ更新しつつ、コピー元で削除されたファイルもコピー先に反映する
function Invoke-MirrorCopy {
    param([string]$Source, [string]$Destination, [string]$Label)

    Write-Host "CopyReleaseAssets: '$Label' を '$Destination' へコピーします"
    robocopy $Source $Destination /MIR /NFL /NDL /NJH /NJS /NC /NS /NP | Out-Null
    # robocopyは0-7が成功（1=コピーあり等）、8以上が失敗
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy がエラー終了しました (exit code $LASTEXITCODE): $Source -> $Destination"
    }
}

try {
    $projectName = Get-LastOpenedProjectName

    if (-not $projectName -or -not (Test-Path -LiteralPath (Join-Path $projectsRoot "$projectName\Project.json"))) {
        # 前回開いたプロジェクトが見つからない場合は、Projects配下の先頭のプロジェクトにフォールバックする
        $fallback = Get-ChildItem -LiteralPath $projectsRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "Project.json") } |
            Select-Object -First 1
        if (-not $fallback) {
            throw "コピー対象のプロジェクトが見つかりません: $projectsRoot"
        }
        $projectName = $fallback.Name
    }

    $sourceAssets = Join-Path $projectsRoot "$projectName\Assets"
    $destAssets = Join-Path $TargetDir "Assets"

    if (-not (Test-Path -LiteralPath $sourceAssets)) {
        throw "プロジェクトのAssetsフォルダが見つかりません: $sourceAssets"
    }

    Invoke-MirrorCopy -Source $sourceAssets -Destination $destAssets -Label "$projectName の Assets"

    # エンジン共通の翻訳（エンジンルート直下 Locales/）も配布形態ではexeと同じフォルダへ同梱する必要がある
    $sourceLocales = Join-Path $ProjectDir "Locales"
    $destLocales = Join-Path $TargetDir "Locales"

    if (Test-Path -LiteralPath $sourceLocales) {
        Invoke-MirrorCopy -Source $sourceLocales -Destination $destLocales -Label "エンジン共通の Locales"
    }

    # 起動スプラッシュ画面（WebView2）のHTML/CSS/JSも、配布形態ではexeと同じフォルダへ同梱する必要がある
    $sourceSplashUI = Join-Path $ProjectDir "KashipanEngine\Splash\UI"
    $destSplashUI = Join-Path $TargetDir "KashipanEngine\Splash\UI"

    if (Test-Path -LiteralPath $sourceSplashUI) {
        Invoke-MirrorCopy -Source $sourceSplashUI -Destination $destSplashUI -Label "起動スプラッシュ画面のUI"
    }

    exit 0
} catch {
    Write-Error $_
    exit 1
}
