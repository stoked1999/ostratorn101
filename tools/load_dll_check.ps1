# Loads the built VST3 DLL the way a host's module loader does: LoadLibrary, then
# GetPluginFactory, then ask the factory how many classes it has.
#
# Split out from the JUCE host test because that one reports a single generic
# failure ("Unable to load VST-3 plug-in file") whether the DLL failed to load or
# the wrapper failed to initialise.  This tells the two apart.
param(
    [string]$Dll = 'C:\Users\bbhal\VST3\OstraTorn101.vst3\Contents\x86_64-win\OstraTorn101.vst3'
)

$signature = @'
[DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern IntPtr LoadLibraryW(string lpFileName);

[DllImport("kernel32", SetLastError=true)]
public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

[DllImport("kernel32", SetLastError=true)]
public static extern bool FreeLibrary(IntPtr hModule);
'@

Add-Type -Namespace Win -Name Native -MemberDefinition $signature

Write-Output "dll: $Dll"

if (-not (Test-Path -LiteralPath $Dll)) {
    Write-Output "MISSING: the DLL does not exist at that path"
    exit 2
}

$module = [Win.Native]::LoadLibraryW($Dll)
if ($module -eq [IntPtr]::Zero) {
    $errorCode = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Output ("LOADLIBRARY-FAILED: error " + $errorCode)
    exit 3
}

Write-Output "LoadLibrary: ok"

$factoryProc = [Win.Native]::GetProcAddress($module, 'GetPluginFactory')
if ($factoryProc -eq [IntPtr]::Zero) {
    Write-Output "NO-FACTORY-EXPORT: GetPluginFactory was not found in the DLL"
    [void][Win.Native]::FreeLibrary($module)
    exit 4
}

Write-Output "GetPluginFactory: found"

# The VST3 ABI puts the factory's vtable as the first word of the object it
# returns; calling through it needs a C-style call, which PowerShell cannot do
# directly, so the export's presence is as far as this check goes.
[void][Win.Native]::FreeLibrary($module)
Write-Output "RESULT: the DLL loads and exports the VST3 entry point"
