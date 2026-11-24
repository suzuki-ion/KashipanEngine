[CmdletBinding()]
param([switch]$Check)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 と .NET Framework だけで動作する。追加のランタイムは不要。
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$rootPrefix = $repositoryRoot.TrimEnd('\') + '\'
$projectPath = Join-Path $repositoryRoot 'Build\KashipanEngine.vcxproj'
$destination = $projectPath + '.filters'
$namespace = 'http://schemas.microsoft.com/developer/msbuild/2003'
$fileTypes = @('ClCompile', 'ClInclude', 'None', 'Text', 'Manifest',
    'ResourceCompile', 'CustomBuild', 'Image', 'FxCompile', 'Natvis')
$pathComparer = [Collections.Generic.Comparer[string]]::Create([Comparison[string]] {
    param($Left, $Right)
    [StringComparer]::Ordinal.Compare($Left.ToLowerInvariant(), $Right.ToLowerInvariant())
})

function Get-FilterIdentifier([string]$Directory) {
    # 従来の Python 生成処理と同じく、URL 名前空間で UUID v5 を生成する。
    [byte[]]$namespaceBytes = @(0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8)
    $nameBytes = [Text.Encoding]::UTF8.GetBytes('KashipanEngine/filters/' + $Directory)
    $sha = [Security.Cryptography.SHA1]::Create()
    try { $hash = $sha.ComputeHash([byte[]]($namespaceBytes + $nameBytes)) }
    finally { $sha.Dispose() }
    $hash[6] = ($hash[6] -band 0x0f) -bor 0x50
    $hash[8] = ($hash[8] -band 0x3f) -bor 0x80
    $hex = ([BitConverter]::ToString($hash[0..15])).Replace('-', '')
    return '{' + $hex.Substring(0, 8) + '-' + $hex.Substring(8, 4) + '-' +
        $hex.Substring(12, 4) + '-' + $hex.Substring(16, 4) + '-' + $hex.Substring(20, 12) + '}'
}

$project = New-Object System.Xml.XmlDocument
$project.XmlResolver = $null
$project.Load($projectPath)
$ns = New-Object System.Xml.XmlNamespaceManager($project.NameTable)
$ns.AddNamespace('m', $namespace)
$items = @{}
$directories = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
$projectDirectory = [IO.Path]::GetDirectoryName($projectPath)
foreach ($node in $project.SelectNodes('/m:Project/m:ItemGroup/*[@Include]', $ns)) {
    $kind = $node.LocalName
    if ($kind -notin $fileTypes) { continue }
    $include = $node.GetAttribute('Include')
    if ($include -match '\$\(|[*?]') { throw "Unsupported project item path: $include" }
    $fullPath = [IO.Path]::GetFullPath((Join-Path $projectDirectory $include))
    if (-not $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Project item is outside the repository: $include"
    }
    $directory = [IO.Path]::GetDirectoryName($fullPath.Substring($rootPrefix.Length))
    $parent = $directory
    while ($parent) {
        [void]$directories.Add($parent)
        $parent = [IO.Path]::GetDirectoryName($parent)
    }
    if (-not $items.ContainsKey($kind)) {
        $items[$kind] = New-Object 'System.Collections.Generic.List[string]'
    }
    $items[$kind].Add($include)
}

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)
$settings.Indent = $true
$settings.IndentChars = '  '
$settings.NewLineChars = "`r`n"
$stream = New-Object System.IO.MemoryStream
$writer = [Xml.XmlWriter]::Create($stream, $settings)
try {
    $writer.WriteStartDocument()
    $writer.WriteStartElement('Project', $namespace)
    $writer.WriteAttributeString('ToolsVersion', '4.0')
    $writer.WriteStartElement('ItemGroup')
    [string[]]$sortedDirectories = @($directories)
    [Array]::Sort($sortedDirectories, $pathComparer)
    foreach ($directory in $sortedDirectories) {
        $writer.WriteStartElement('Filter')
        $writer.WriteAttributeString('Include', $directory)
        $writer.WriteElementString('UniqueIdentifier', (Get-FilterIdentifier $directory))
        $writer.WriteEndElement()
    }
    $writer.WriteEndElement()
    [string[]]$sortedKinds = @($items.Keys)
    [Array]::Sort($sortedKinds, [StringComparer]::Ordinal)
    foreach ($kind in $sortedKinds) {
        $writer.WriteStartElement('ItemGroup')
        $items[$kind].Sort($pathComparer)
        foreach ($include in $items[$kind]) {
            $fullPath = [IO.Path]::GetFullPath((Join-Path $projectDirectory $include))
            $directory = [IO.Path]::GetDirectoryName($fullPath.Substring($rootPrefix.Length))
            $writer.WriteStartElement($kind)
            $writer.WriteAttributeString('Include', $include)
            if ($directory) { $writer.WriteElementString('Filter', $directory) }
            $writer.WriteEndElement()
        }
        $writer.WriteEndElement()
    }
    $writer.WriteEndElement()
    $writer.WriteEndDocument()
    $writer.Flush()
    $expected = [Text.Encoding]::UTF8.GetString($stream.ToArray()) + "`r`n"
}
finally { $writer.Dispose(); $stream.Dispose() }

$current = if ([IO.File]::Exists($destination)) { [IO.File]::ReadAllText($destination) } else { $null }
$matches = $null -ne $current -and $current.Replace("`r`n", "`n") -ceq $expected.Replace("`r`n", "`n")
if ($Check) {
    if (-not $matches) {
        Write-Error 'Filters are out of date. Run EngineTools/SyncVisualStudioFilters.ps1.'
        exit 1
    }
    Write-Output 'Visual Studio filters match the registered files and repository directories.'
}
elseif ($matches) { Write-Output 'Visual Studio filters are already up to date.' }
else {
    [IO.File]::WriteAllText($destination, $expected, $settings.Encoding)
    Write-Output 'Updated Build\KashipanEngine.vcxproj.filters'
}
