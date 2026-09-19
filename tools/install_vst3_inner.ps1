# Inner (elevated) step: copies the built VST3 bundle into a target VST3 folder.
# Writes a result file the non-elevated side can read, so the outcome is
# verifiable rather than assumed.
#
# Usage (from the outer script): -Destination 'C:\Program Files\Common Files\VST3'
param(
    [string]$Destination = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'
$src = 'C:\Users\bbhal\VST3\SH-101.vst3'
$dst = Join-Path $Destination 'SH-101.vst3'
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'

try {
    if (-not (Test-Path -LiteralPath $src)) { throw "source bundle not found: $src" }
    if (-not (Test-Path -LiteralPath $Destination)) { throw "VST3 folder not found: $Destination" }

    if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force }
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force

    $dll = Join-Path $dst 'Contents\x86_64-win\SH-101.vst3'
    if (-not (Test-Path -LiteralPath $dll)) { throw "copy completed but the plugin DLL is missing" }

    $size = (Get-Item -LiteralPath $dll).Length
    "OK installed=$dst dll_bytes=$size at=$(Get-Date -Format s)" | Set-Content -LiteralPath $resultFile
}
catch {
    "FAILED: $($_.Exception.Message)" | Set-Content -LiteralPath $resultFile
}
