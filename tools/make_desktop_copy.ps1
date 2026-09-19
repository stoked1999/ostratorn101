# Packs the built VST3 bundle plus its install notes into a zip on the Desktop,
# ready to hand to someone else.
#
# The bundle is a folder tree (Contents/x86_64-win/...), so it is zipped rather
# than copied loose: mail, chat apps and file shares all handle one file better
# than a directory, and the unzipped result is exactly what goes into a VST3
# folder.
param(
    [string]$Source = 'C:\Users\bbhal\sh101\build-plugin-msvc\src\plugin\juce\SH101Plugin_artefacts\Release\VST3\OstraTorn101.vst3',
    [string]$Notes = 'C:\Users\bbhal\sh101\packaging\INSTALL.txt'
)

$ErrorActionPreference = 'Stop'

$desktop = [Environment]::GetFolderPath('Desktop')
if ([string]::IsNullOrWhiteSpace($desktop)) { throw 'could not locate the Desktop folder' }

Write-Output "desktop: $desktop"
Write-Output "source:  $Source"

if (-not (Test-Path -LiteralPath $Source)) { throw "bundle not found: $Source" }
if (-not (Test-Path -LiteralPath $Notes)) { throw "install notes not found: $Notes" }

$innerDll = Join-Path $Source 'Contents\x86_64-win\OstraTorn101.vst3'
if (-not (Test-Path -LiteralPath $innerDll)) { throw 'the bundle is missing its plugin DLL' }
$dllBytes = (Get-Item -LiteralPath $innerDll).Length

# Stage, so the zip holds the bundle folder *and* the notes side by side.
$staging = Join-Path $env:TEMP ('ot101-pack-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $staging | Out-Null

try {
    Copy-Item -LiteralPath $Source -Destination $staging -Recurse -Force
    Copy-Item -LiteralPath $Notes -Destination (Join-Path $staging 'INSTALL.txt') -Force

    $zip = Join-Path $desktop 'OstraTorn101-vst3-win64.zip'
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }

    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip -CompressionLevel Optimal

    $zipBytes = (Get-Item -LiteralPath $zip).Length
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $entries = [System.IO.Compression.ZipFile]::OpenRead($zip).Entries.Count

    Write-Output "zip:     $zip"
    Write-Output ("size:    {0:N0} bytes  (plugin DLL was {1:N0} bytes)" -f $zipBytes, $dllBytes)
    Write-Output "entries: $entries"
    Write-Output 'RESULT: desktop copy created'
}
finally {
    Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
}
