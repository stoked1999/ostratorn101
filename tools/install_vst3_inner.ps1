# Inner (elevated) step: copies one built VST3 bundle into a target VST3 folder.
# Writes a result file the non-elevated side can read, so the outcome is
# verifiable rather than assumed.
#
# Usage (from the outer script):
#   -SourceBundle '<...>\SH101Plugin_artefacts\Release\VST3\OstraTorn101.vst3'
#   -Destination  'C:\Program Files\Common Files\VST3'
#
# The source defaults to the build output, NOT to a staging folder: installing a
# copy that nobody refreshed is exactly how a stale build gets shipped while the
# installer still reports success (the staged copy is refreshed by the build).
param(
    [string]$SourceBundle = 'C:\Users\bbhal\sh101\build-plugin-msvc\src\plugin\juce\SH101Plugin_artefacts\Release\VST3\OstraTorn101.vst3',
    [string]$Destination = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'

# The bundle name is ASCII; the instrument's displayed name (which contains a
# non-ASCII letter) is compiled into the plugin, not derived from the file name.
$bundleName = 'OstraTorn101.vst3'

$src = $SourceBundle
$dst = Join-Path $Destination $bundleName
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'

try {
    if (-not (Test-Path -LiteralPath $src)) { throw "source bundle not found: $src - build the plugin first" }
    $srcDll = Join-Path $src ('Contents\x86_64-win\' + $bundleName)
    if (-not (Test-Path -LiteralPath $srcDll)) { throw "source bundle has no plugin DLL: $srcDll" }
    $srcHash = (Get-FileHash -LiteralPath $srcDll -Algorithm SHA256).Hash

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

    # What was written must be what was built: anything else is a stale install
    # wearing a success message.
    $installedHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ($installedHash -ne $srcHash) {
        throw "installed copy differs from the build output (source $srcHash, installed $installedHash)"
    }

    $size = (Get-Item -LiteralPath $dll).Length
    # Append, never overwrite: two destinations report into the same file, and a
    # failure in the first must not be hidden by the second one's success.
    "OK installed=$dst dll_bytes=$size sha256=$($installedHash.Substring(0,16)) from=$src at=$(Get-Date -Format s)" |
        Add-Content -LiteralPath $resultFile
}
catch {
    "FAILED destination=$Destination reason=$($_.Exception.Message)" | Add-Content -LiteralPath $resultFile
}
