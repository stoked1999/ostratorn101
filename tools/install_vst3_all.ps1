# Installs the built SH-101 VST3 into both of Live's VST3 folders in one go.
#
# Run from bash:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3_all.ps1
#
# One UAC prompt appears; approve it and both copies are written.
$inner = 'C:\Users\bbhal\sh101\tools\install_vst3_inner.ps1'
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'

if (-not (Test-Path -LiteralPath $inner)) {
    Write-Output "ERROR: helper script not found: $inner"
    exit 2
}

if (Test-Path -LiteralPath $resultFile) { Remove-Item -LiteralPath $resultFile -Force }

# Both destinations are handled by separate inner invocations inside ONE elevated
# process, so the user only sees a single consent prompt.
$inner64 = 'C:\Program Files\Common Files\VST3'
$inner86 = 'C:\Program Files (x86)\Common Files\VST3'
$command = "& '$inner' -Destination '$inner64'; & '$inner' -Destination '$inner86'"

try {
    $proc = Start-Process -FilePath 'powershell.exe' `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $command) `
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
