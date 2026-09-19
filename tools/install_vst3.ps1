# Installs the built SH-101 VST3 into a VST3 folder (needs admin rights).
#
# Run from bash:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3.ps1 -Destination 'C:\Program Files (x86)\Common Files\VST3'
#
# A UAC prompt appears; approving it is the only user action required.
#
# Note: the "(x86)" folder is the 32-bit VST3 location.  Ableton Live 10+ has no
# 32-bit plugin support, so a copy there is inert as far as Live is concerned; it
# exists only for tidiness/completeness.  The 64-bit folder is the one Live scans.
param(
    [string]$Destination = 'C:\Program Files\Common Files\VST3'
)

$inner = 'C:\Users\bbhal\sh101\tools\install_vst3_inner.ps1'
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'

if (-not (Test-Path -LiteralPath $inner)) {
    Write-Output "ERROR: helper script not found: $inner"
    exit 2
}

# Clear any previous result so we cannot read a stale outcome.
if (Test-Path -LiteralPath $resultFile) { Remove-Item -LiteralPath $resultFile -Force }

Write-Output "destination: $Destination"

try {
    # Start-Process joins the argument array with spaces, so a destination path
    # containing spaces must be quoted explicitly or it is split into pieces.
    $quotedDestination = '"' + $Destination + '"'
    $proc = Start-Process -FilePath 'powershell.exe' `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $inner,
                        '-Destination', $quotedDestination) `
        -Verb RunAs -PassThru -Wait -ErrorAction Stop
    Write-Output ("ELEVATED-EXITCODE=" + $proc.ExitCode)
} catch {
    Write-Output ("ELEVATION-FAILED: " + $_.Exception.Message)
    exit 3
}

if (Test-Path -LiteralPath $resultFile) {
    Write-Output (Get-Content -LiteralPath $resultFile -Raw)
} else {
    Write-Output "NO-RESULT-FILE (the elevated step did not report back)"
}
