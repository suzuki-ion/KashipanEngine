[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [Parameter(Mandatory = $true)]
    [string]$Configuration
)

$ErrorActionPreference = "Stop"

# Windows PowerShell(5.1)は既定でTLS1.2が無効な場合があり、GitHubへの接続が
# サイレントに失敗することがあるため明示的に有効化する。
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

# $(ProjectDir) は末尾が "\" のため、末尾に "." を付けて渡されている。
# ここで取り除いて実際のフォルダパスに戻す。
function Remove-TrailingDot {
    param([string]$Path)
    return $Path.TrimEnd('.').TrimEnd('\')
}

$ProjectDir = Remove-TrailingDot $ProjectDir

# Assimpの静的ライブラリの配布元（GitHub Release）。
# ファイルを差し替えた場合はタグ・サイズをここも合わせて更新する。
$releaseTag = "assimp-libs-v1"
$baseUrl = "https://github.com/suzuki-ion/KashipanEngine/releases/download/$releaseTag"

# 構成ごとに必要なライブラリのみをダウンロードする（Debug構成でRelease用の
# ライブラリまで取得する、といった無駄なダウンロードを避けるため）
if ($Configuration -eq "Debug") {
    $fileName = "assimp-vc143-mtd.lib"
    $subDir = "Debug"
    $expectedSize = 168593450
} else {
    $fileName = "assimp-vc143-mt.lib"
    $subDir = "Release"
    $expectedSize = 75762326
}

$destDir = Join-Path $ProjectDir "Externals\Assimp\lib\$subDir"
$destPath = Join-Path $destDir $fileName

if (Test-Path -LiteralPath $destPath) {
    $existing = Get-Item -LiteralPath $destPath
    if ($existing.Length -eq $expectedSize) {
        Write-Host "DownloadAssimpLibs: '$fileName' は取得済みのためスキップします"
        exit 0
    }
    Write-Host "DownloadAssimpLibs: '$fileName' のサイズが期待値と異なるため再取得します"
}

if (-not (Test-Path -LiteralPath $destDir)) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$url = "$baseUrl/$fileName"
$tempPath = "$destPath.download"
Write-Host "DownloadAssimpLibs: '$url' から '$destPath' へダウンロードします"

try {
    Invoke-WebRequest -Uri $url -OutFile $tempPath -UseBasicParsing -ErrorAction Stop
} catch {
    if (Test-Path -LiteralPath $tempPath) { Remove-Item -LiteralPath $tempPath -Force }
    throw "DownloadAssimpLibs: ダウンロードに失敗しました ($url): $($_.Exception.Message)"
}

$downloaded = Get-Item -LiteralPath $tempPath
if ($downloaded.Length -ne $expectedSize) {
    Remove-Item -LiteralPath $tempPath -Force
    throw "DownloadAssimpLibs: ダウンロードしたファイルのサイズが一致しません (期待値 $expectedSize, 実際 $($downloaded.Length))"
}

Move-Item -LiteralPath $tempPath -Destination $destPath -Force
