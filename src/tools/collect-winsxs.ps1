param(
    [Parameter(Mandatory = $true)][string]$WinSxsDestination
)

# Copies the side-by-side assemblies that desktop applications bind to through their manifests
# (common controls 6, GDI+ 1.1, the VC++ 8/9 runtimes) into the emulation root, together with their
# publisher policies. Windows keeps WinSxS manifests delta-compressed ("DCM" header, PA30 delta against
# a dictionary stored in the servicing stack's wcp.dll); they are written out as plain XML.

$ErrorActionPreference = 'Stop'

$assemblyPatterns = @(
    'microsoft.windows.common-controls_*',
    'microsoft.windows.gdiplus_*',
    'microsoft.vc80.*',
    'microsoft.vc90.*',
    'policy.*.microsoft.windows.common-controls_*',
    'policy.*.microsoft.windows.gdiplus_*',
    'policy.*.microsoft.vc80.*',
    'policy.*.microsoft.vc90.*'
)

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class SxsManifest
{
    [StructLayout(LayoutKind.Sequential)]
    struct DeltaInput { public IntPtr Start; public UIntPtr Size; public int Editable; }

    [StructLayout(LayoutKind.Sequential)]
    struct DeltaOutput { public IntPtr Start; public UIntPtr Size; }

    [DllImport("msdelta.dll", SetLastError = true)]
    static extern bool ApplyDeltaB(long flags, DeltaInput source, DeltaInput delta, out DeltaOutput target);

    [DllImport("msdelta.dll")]
    static extern bool DeltaFree(IntPtr memory);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr LoadResource(IntPtr module, IntPtr resource);

    [DllImport("kernel32.dll")]
    static extern IntPtr LockResource(IntPtr data);

    [DllImport("kernel32.dll")]
    static extern uint SizeofResource(IntPtr module, IntPtr resource);

    static IntPtr dictionary;
    static uint dictionarySize;

    public static void LoadDictionary(string wcpPath)
    {
        IntPtr module = LoadLibraryExW(wcpPath, IntPtr.Zero, 0x2);
        if (module == IntPtr.Zero) throw new Exception("LoadLibraryEx(wcp.dll) failed: " + Marshal.GetLastWin32Error());
        IntPtr resource = FindResourceW(module, (IntPtr)1, (IntPtr)0x266);
        if (resource == IntPtr.Zero) throw new Exception("wcp.dll has no manifest dictionary resource");
        dictionary = LockResource(LoadResource(module, resource));
        dictionarySize = SizeofResource(module, resource);
    }

    public static byte[] Expand(byte[] data)
    {
        if (data.Length < 4 || data[0] != (byte)'D' || data[1] != (byte)'C' || data[2] != (byte)'M' || data[3] != 1)
        {
            return data;
        }

        GCHandle pinned = GCHandle.Alloc(data, GCHandleType.Pinned);
        try
        {
            DeltaInput source = new DeltaInput { Start = dictionary, Size = (UIntPtr)dictionarySize, Editable = 0 };
            DeltaInput delta = new DeltaInput { Start = pinned.AddrOfPinnedObject() + 4, Size = (UIntPtr)(data.Length - 4), Editable = 0 };
            DeltaOutput target;
            if (!ApplyDeltaB(0, source, delta, out target)) throw new Exception("ApplyDeltaB failed: " + Marshal.GetLastWin32Error());
            byte[] result = new byte[(int)target.Size];
            Marshal.Copy(target.Start, result, 0, result.Length);
            DeltaFree(target.Start);
            return result;
        }
        finally
        {
            pinned.Free();
        }
    }
}
'@

$winSxs = Join-Path $env:WINDIR 'WinSxS'
$wcp = Get-ChildItem -Path (Join-Path $winSxs 'amd64_microsoft-windows-servicingstack_*\wcp.dll') |
    Sort-Object LastWriteTime | Select-Object -Last 1
[SxsManifest]::LoadDictionary($wcp.FullName)

$manifestDestination = Join-Path $WinSxsDestination 'manifests'
New-Item -ItemType Directory -Force -Path $manifestDestination | Out-Null

foreach ($architecture in @('x86', 'amd64')) {
    foreach ($pattern in $assemblyPatterns) {
        $name = "${architecture}_$pattern"

        foreach ($manifest in Get-ChildItem -Path (Join-Path $winSxs 'Manifests') -Filter "$name.manifest") {
            $expanded = [SxsManifest]::Expand([IO.File]::ReadAllBytes($manifest.FullName))
            [IO.File]::WriteAllBytes((Join-Path $manifestDestination $manifest.Name), $expanded)
        }

        foreach ($payload in Get-ChildItem -Path $winSxs -Directory -Filter $name) {
            Copy-Item -Recurse -Force -Path $payload.FullName -Destination (Join-Path $WinSxsDestination $payload.Name)
        }
    }
}
