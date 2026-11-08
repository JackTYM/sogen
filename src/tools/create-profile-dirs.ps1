# Create directory structure under filesys root for each user in ProfileList.
# Usage: .\create-profile-dirs.ps1 -FilesysRoot "root\filesys"
# ProfileImagePath is read, env vars expanded, path lowercased, then dirs created.

param(
    [Parameter(Mandatory = $true)]
    [string] $FilesysRoot
)

$profileListPath = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList'
Get-ChildItem $profileListPath -ErrorAction SilentlyContinue | ForEach-Object {
    $p = (Get-ItemProperty -Path $_.PSPath -Name ProfileImagePath -ErrorAction SilentlyContinue).ProfileImagePath
    if ($p) {
        $exp = [Environment]::ExpandEnvironmentVariables($p)
        $low = $exp.ToLower()
        $rel = $low -replace '^([a-z]):', '$1'
        $target = Join-Path $FilesysRoot $rel
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        Write-Host 'Created:' $target
        $appData = Join-Path $target 'appdata'
        @('local', 'roaming', 'locallow') | ForEach-Object {
            $sub = Join-Path $appData $_
            New-Item -ItemType Directory -Path $sub -Force | Out-Null
            Write-Host 'Created:' $sub
        }
    }
}

# C:\Users\Public is not a profile of ProfileList but the "Public" value of that key; known-folder lookups (FOLDERID_Public) and
# installer custom actions that save files there fail without the directory tree.
$pub = (Get-ItemProperty -Path $profileListPath -Name Public -ErrorAction SilentlyContinue).Public
if ($pub) {
    $low = ([Environment]::ExpandEnvironmentVariables($pub)).ToLower() -replace '^([a-z]):', '$1'
    $target = Join-Path $FilesysRoot $low
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Write-Host 'Created:' $target
    @('desktop', 'documents', 'downloads', 'music', 'pictures', 'videos') | ForEach-Object {
        New-Item -ItemType Directory -Path (Join-Path $target $_) -Force | Out-Null
    }
}
