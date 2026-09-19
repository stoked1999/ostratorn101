# Requests the VS installer to add the "Desktop development with C++" workload.
# Run from bash as:  powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_cpp_workload.ps1
#
# -Verb RunAs raises the UAC prompt; if the prompt is declined, PowerShell throws
# and this script reports it instead of failing silently.
$vsInstaller = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe'
$installPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$vsArgs = @(
    'modify',
    '--installPath', $installPath,
    '--add', 'Microsoft.VisualStudio.Workload.NativeDesktop',
    '--includeRecommended',
    '--passive',
    '--norestart'
)

if (-not (Test-Path $vsInstaller)) {
    Write-Output "ERROR: VS installer not found at $vsInstaller"
    exit 2
}

try {
    $proc = Start-Process -FilePath $vsInstaller -ArgumentList $vsArgs -Verb RunAs -PassThru -ErrorAction Stop
    Write-Output ("ELEVATION-ACCEPTED pid=" + $proc.Id)
} catch {
    Write-Output ("ELEVATION-FAILED: " + $_.Exception.Message)
    exit 3
}
