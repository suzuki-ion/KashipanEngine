[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [Parameter(Mandatory = $true)]
    [string]$TargetDir,

    [Parameter(Mandatory = $true)]
    [string]$Platform,

    [Parameter(Mandatory = $true)]
    [string]$Configuration
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

# ビルド中でもexeを実行し続けられる、通常のビルド出力先(TargetDir)とは別の作業用コピー先。
# TargetDirはビルドのたびにリンカが書き換えるため、そこで作業していると再ビルド時にexeがロックされて失敗する。
# ここへは「ビルドが完了した後」の成果物をまとめてミラーコピーするだけなので、
# WorkCopy側のexeを実行中でも本来のビルド(TargetDir)には影響しない。
$destDir = Join-Path $ProjectDir "WorkCopy\$Platform\$Configuration"

try {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null

    Write-Host "CopyToWorkCopy: '$TargetDir' を '$destDir' へコピーします"
    robocopy $TargetDir $destDir /MIR /NFL /NDL /NJH /NJS /NC /NS /NP | Out-Null
    $code = $LASTEXITCODE

    # robocopyは0-7が成功（1=コピーあり等）、8=一部ファイルのコピー失敗（ロック中等）、16以上は致命的エラー。
    # WorkCopy側のexeを実行中の場合は8になり得るが、これはビルド失敗にはしない。
    if ($code -ge 16) {
        throw "robocopy が致命的エラーで終了しました (exit code $code): $TargetDir -> $destDir"
    } elseif ($code -ge 8) {
        Write-Warning "作業用コピー先の一部ファイルをコピーできませんでした（実行中でロックされている可能性があります）: $destDir"
    }

    exit 0
} catch {
    Write-Error $_
    exit 1
}
