# Installs the built SH-101 VST3 into both of Live's VST3 folders in one go, and
# verifies every copy against the build output by hash.
#
# Run from bash:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3_all.ps1
#
# One UAC prompt appears; approve it and both copies are written.
param(
    [string]$SourceBundle = 'C:\Users\bbhal\sh101\build-plugin-msvc\src\plugin\juce\SH101Plugin_artefacts\Release\VST3\OstraTorn101.vst3'
)

$inner = 'C:\Users\bbhal\sh101\tools\install_vst3_inner.ps1'
$resultFile = 'C:\Users\bbhal\VST3\install-result.txt'
$bundleName = 'OstraTorn101.vst3'

if (-not (Test-Path -LiteralPath $inner)) {
    Write-Output "ERROR: helper script not found: $inner"
    exit 2
}

$srcDll = Join-Path $SourceBundle ('Contents\x86_64-win\' + $bundleName)
if (-not (Test-Path -LiteralPath $srcDll)) {
    Write-Output "ERROR: no built plugin at $srcDll"
    Write-Output "       build it first:  tools\build_vst3_msvc.bat"
    exit 2
}
$srcHash = (Get-FileHash -LiteralPath $srcDll -Algorithm SHA256).Hash
Write-Output ("SOURCE $SourceBundle")
Write-Output ("SOURCE bytes=$((Get-Item -LiteralPath $srcDll).Length) sha256=$($srcHash.Substring(0,16))")

# A running DAW keeps the installed plugin loaded, and the copy then fails with a
# bare "access denied" that reads like a permissions problem.  Name the cause.
$dawNames = @('Ableton Live 12 Standard', 'Ableton Live 12 Suite', 'Ableton Live 12 Intro',
              'Ableton Live 11 Standard', 'Ableton Live 11 Suite', 'Ableton Live',
              'reaper', 'Reaper', 'Bitwig Studio', 'Cubase12', 'FL64')
$runningDaws = @(Get-Process -ErrorAction SilentlyContinue |
                 Where-Object { $dawNames -contains $_.ProcessName } |
                 Select-Object -ExpandProperty ProcessName -Unique)
if ($runningDaws.Count -gt 0) {
    Write-Output ("WARNING: a DAW is running (" + ($runningDaws -join ', ') + ").")
    Write-Output "         It holds the installed plugin open, so replacing it fails with"
    Write-Output "         'access denied'.  Quit it first, then run this again."
}

if (Test-Path -LiteralPath $resultFile) { Remove-Item -LiteralPath $resultFile -Force }

# Both destinations are handled by separate inner invocations inside ONE elevated
# process, so the user only sees a single consent prompt.
$destinations = @(
    'C:\Program Files\Common Files\VST3',
    'C:\Program Files (x86)\Common Files\VST3'
)
$command = (($destinations | ForEach-Object {
    "& '$inner' -SourceBundle '$SourceBundle' -Destination '$_'"
}) -join '; ')

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

# Independent check from this side: every destination must hold exactly the bytes
# that were built, so a stale or partial copy cannot pass as an install.
$ok = $true
foreach ($destination in $destinations) {
    $dll = Join-Path (Join-Path $destination $bundleName) ('Contents\x86_64-win\' + $bundleName)
    if (-not (Test-Path -LiteralPath $dll)) {
        Write-Output "VERIFY MISSING $dll"
        $ok = $false
        continue
    }
    $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    if ($hash -eq $srcHash) {
        Write-Output "VERIFY OK      $dll"
    } else {
        Write-Output "VERIFY MISMATCH $dll (installed $($hash.Substring(0,16)), built $($srcHash.Substring(0,16)))"
        $ok = $false
    }
}
if (-not $ok) { exit 4 }
