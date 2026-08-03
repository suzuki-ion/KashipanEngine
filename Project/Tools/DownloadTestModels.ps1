[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir
)

$ErrorActionPreference = "Stop"

# $(ProjectDir) は末尾が "\" のため、末尾に "." を付けて渡されている。
# ここで取り除いて実際のフォルダパスに戻す。
function Remove-TrailingDot {
    param([string]$Path)
    return $Path.TrimEnd('.').TrimEnd('\')
}

$ProjectDir = Remove-TrailingDot $ProjectDir

# テスト用3Dモデルの配布元（GitHub Release）。
# ファイルを差し替えた場合はタグ・サイズをここも合わせて更新する。
$releaseTag = "test-models-v1"
$baseUrl = "https://github.com/suzuki-ion/KashipanEngine/releases/download/$releaseTag"

$fileName = "living_room.obj"
$expectedSize = 55324031
$destDir = Join-Path $ProjectDir "Projects\JobHuntingGame\Assets\Application\Model\TestModels\living_room"
$destPath = Join-Path $destDir $fileName

if (Test-Path -LiteralPath $destPath) {
    $existing = Get-Item -LiteralPath $destPath
    if ($existing.Length -eq $expectedSize) {
        Write-Host "DownloadTestModels: '$fileName' は取得済みのためスキップします"
        exit 0
    }
    Write-Host "DownloadTestModels: '$fileName' のサイズが期待値と異なるため再取得します"
}

if (-not (Test-Path -LiteralPath $destDir)) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$url = "$baseUrl/$fileName"
$tempPath = "$destPath.download"
Write-Host "DownloadTestModels: '$url' から '$destPath' へダウンロードします"

try {
    Invoke-WebRequest -Uri $url -OutFile $tempPath -UseBasicParsing
} catch {
    if (Test-Path -LiteralPath $tempPath) { Remove-Item -LiteralPath $tempPath -Force }
    throw "DownloadTestModels: ダウンロードに失敗しました ($url): $_"
}

$downloaded = Get-Item -LiteralPath $tempPath
if ($downloaded.Length -ne $expectedSize) {
    Remove-Item -LiteralPath $tempPath -Force
    throw "DownloadTestModels: ダウンロードしたファイルのサイズが一致しません (期待値 $expectedSize, 実際 $($downloaded.Length))"
}

Move-Item -LiteralPath $tempPath -Destination $destPath -Force
