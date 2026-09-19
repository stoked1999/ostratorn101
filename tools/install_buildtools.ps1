# Installs the VS 2022 C++ Build Tools workload (MSVC + Windows SDK) using the
# standalone bootstrapper, independently of the Community instance.
#
# Run from bash:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_buildtools.ps1
#
# -Verb RunAs raises the UAC prompt; a declined prompt is reported rather than
# failing silently (the VS Installer's `modify` path on this machine exits with
# code 1 immediately, which is why the bootstrapper route is used).
$bootstrapper = 'C:\Users\bbhal\vsbt\vs_BuildTools.exe'
$vsArgs = @(
    '--passive',
    '--wait',
    '--norestart',
    '--add', 'Microsoft.VisualStudio.Workload.VCTools',
    '--includeRecommended'
)

if (-not (Test-Path $bootstrapper)) {
    Write-Output "ERROR: bootstrapper not found at $bootstrapper"
    exit 2
}

try {
    $proc = Start-Process -FilePath $bootstrapper -ArgumentList $vsArgs -Verb RunAs -PassThru -ErrorAction Stop
    Write-Output ("ELEVATION-ACCEPTED pid=" + $proc.Id)
} catch {
    Write-Output ("ELEVATION-FAILED: " + $_.Exception.Message)
    exit 3
}

# The bootstrapper itself exits once it has handed over to the installer, so poll
# for the toolset appearing on disk instead of waiting on one process.
$msvc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC'
$sdk = 'C:\Program Files (x86)\Windows Kits\10\Include'
$deadline = (Get-Date).AddMinutes(45)
while ((Get-Date) -lt $deadline) {
    if ((Test-Path $msvc) -and (Test-Path $sdk)) {
        Write-Output "TOOLCHAIN-READY"
        exit 0
    }
    Start-Sleep -Seconds 20
}
Write-Output "TIMEOUT-WAITING-FOR-TOOLCHAIN"
exit 1
