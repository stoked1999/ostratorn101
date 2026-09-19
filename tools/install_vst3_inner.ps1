# Inner (elevated) step: copies the built VST3 bundle into a target VST3 folder.
# Writes a result file the non-elevated side can read, so the outcome is
# verifiable rather than assumed.
#
# Usage (from the outer script): -Destination 'C:\Program Files\Common Files\VST3'
param(
    [string]$Destination = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'

# The bundle name is ASCII; the instrument's displayed name (which contains a
# non-ASCII letter) is compiled into the plugin, not derived from the file name.
$bundleName = 'OstraTorn101.vst3'

$src = Join-Path 'C:\Users\bbhal\VST3' $bundleName
$dst = Join-Path $Destination $bundleName
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'

try {
    if (-not (Test-Path -LiteralPath $src)) { throw "source bundle not found: $src" }
    if (-not (Test-Path -LiteralPath $Destination)) { throw "VST3 folder not found: $Destination" }

    if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force }
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force

    # Remove bundles from earlier names, so a host does not list several
    # instruments that are the same plugin.
    foreach ($obsoleteName in @('SH-101.vst3', ([string][char]0x00D6 + 'straTorn101.vst3'))) {
        $obsolete = Join-Path $Destination $obsoleteName
        if (Test-Path -LiteralPath $obsolete) { Remove-Item -LiteralPath $obsolete -Recurse -Force }
    }

    $dll = Join-Path $dst ('Contents\x86_64-win\' + $bundleName)
    if (-not (Test-Path -LiteralPath $dll)) { throw "copy completed but the plugin DLL is missing" }

    $size = (Get-Item -LiteralPath $dll).Length
    "OK installed=$dst dll_bytes=$size at=$(Get-Date -Format s)" | Set-Content -LiteralPath $resultFile
}
catch {
    "FAILED: $($_.Exception.Message)" | Set-Content -LiteralPath $resultFile
}
