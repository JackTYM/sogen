# Capture the property system's schema cache.
# Usage: .\collect-property-cache.ps1 -FilesysRoot "root\filesys"
# propsys.dll refuses to start (PSGetPropertyDescription* fail with E_OUTOFMEMORY, which stops the shell's folder
# views) unless the version cache files it reads from "Microsoft\Windows\Caches" already exist. A call into the
# property system makes this machine create them; they are then copied next to the emulated profile.

param(
    [Parameter(Mandatory = $true)]
    [string] $FilesysRoot
)

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class PropSysProbe
{
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSGetPropertyDescriptionByName(string name, ref Guid riid, out IntPtr ppv);
}
'@

$iidPropertyDescription = [Guid] '6F79D558-3E96-4549-A1D1-7D75D2288814'
foreach ($name in @('System.ItemNameDisplay', 'System.DateModified', 'System.ItemTypeText', 'System.Size')) {
    $ppv = [IntPtr]::Zero
    $hr = [PropSysProbe]::PSGetPropertyDescriptionByName($name, [ref] $iidPropertyDescription, [ref] $ppv)
    Write-Host ('PSGetPropertyDescriptionByName({0}) -> 0x{1:X8}' -f $name, $hr)
}

function Copy-Caches($source, $relativeTarget) {
    if (-not (Test-Path $source)) {
        Write-Host 'Missing:' $source
        return
    }
    $target = Join-Path $FilesysRoot $relativeTarget.ToLower()
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Get-ChildItem -Path $source -File -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination (Join-Path $target $_.Name) -Force -ErrorAction SilentlyContinue
        Write-Host $_.FullName '->' $target
    }
}

Copy-Caches (Join-Path $env:ProgramData 'Microsoft\Windows\Caches') 'c\ProgramData\Microsoft\Windows\Caches'

$profileRelative = ([Environment]::ExpandEnvironmentVariables($env:USERPROFILE)) -replace '^([A-Za-z]):', '$1'
Copy-Caches (Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\Caches') (Join-Path $profileRelative 'AppData\Local\Microsoft\Windows\Caches')
