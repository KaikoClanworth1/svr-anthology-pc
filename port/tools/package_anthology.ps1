# The SvR Anthology release (branch anthology): SVR Anthology by Yumboy11 on the PC recomp. No game data and
# nothing of the Anthology - the player uses their own ISO and Yumboy11's own installer (Read Me).
#   .\package_anthology.ps1 -Base <the normal release's package folder> [-Version 1.0.0]
#     -> port\out\SvR-Anthology-PC-v<version>.zip
# -Base: the normal release of the same version (port\out\package\SvR2011-PC-v<version> in the release
# copy): its shaders, button pictures and third-party notices are the same files. The programs come from
# this branch's build (out\build\SourceAnth: an_build_low.cmd --target svr2011 svr2011_anthology_launcher).
param([Parameter(Mandatory = $true)][string]$Base, [string]$Version = "", [string]$Build = "")
$ErrorActionPreference = "Stop"
$port = Split-Path $PSScriptRoot -Parent
if (-not $Version) { $Version = (Get-Content (Join-Path $port "ANTHOLOGY_VERSION.txt") -TotalCount 1).Trim() }
$build = if ($Build) { $Build } else { Join-Path $port "out\build\SourceAnth" }
$sdk = Join-Path (Split-Path $port -Parent) "recomp\rexglue-sdk\out\win-amd64"
$name = "SvR-Anthology-PC-v$Version"
$stage = Join-Path $port "out\package\$name"
$zip = Join-Path $port "out\$name.zip"
if (-not (Test-Path (Join-Path $Base "native_shaders\shaders.dxil.pak"))) { throw "no native_shaders\shaders.dxil.pak in $Base" }
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
# the programs: the Anthology build's game and launcher, the SDK's DLLs it was built with
foreach ($f in @((Join-Path $build "svr2011.exe"), (Join-Path $build "SvR Anthology Launcher.exe"),
                 (Join-Path $sdk "rexruntime.dll"), (Join-Path $sdk "rexgpu-xenos.dll"))) {
    if (-not (Test-Path $f)) { throw "$f is missing - build first" }
    Copy-Item $f $stage
}
# the same files as the normal release
Copy-Item (Join-Path $Base "native_shaders") $stage -Recurse
Copy-Item (Join-Path $Base "pad_icons") $stage -Recurse
Copy-Item (Join-Path $Base "Third-party notices.txt") $stage
foreach ($f in @("nvngx_dlss.dll", "NVIDIA DLSS licence.txt")) {   # (DLSS, experimental: only with its licence)
    if (Test-Path (Join-Path $Base $f)) { Copy-Item (Join-Path $Base $f) $stage }
}
Copy-Item (Join-Path $port "dist\anthology\Read Me.txt") $stage
# the Android app (tools/build_apk.py in this branch: the SvR Anthology app), for the launcher's Phone page
$apk = Join-Path $port "out\android\SvR Anthology.apk"
if (-not (Test-Path $apk)) { throw "$apk is missing - build the app first (tools\build_apk.py)" }
New-Item -ItemType Directory -Force (Join-Path $stage "Android") | Out-Null
Copy-Item $apk (Join-Path $stage "Android")
# never: mods, the Mod Maker, the normal launcher / updater, the VC++ runtime, game files, Anthology files
foreach ($bad in @("SvR2011 Launcher.exe", "SvR2011 Updater.exe", "SvR2011 Mod Maker.exe", "Mod Maker Tools", "Bundled Mods",
                   "default.xex", "pac", "Original", "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
    if (Test-Path (Join-Path $stage $bad)) { throw "$bad is in the Anthology release folder" }
}
$listFile = Join-Path $stage "release_files.txt"
$rootLen = (Resolve-Path $stage).Path.TrimEnd('\').Length + 1
$lines = @("SvR Anthology release $Version")
foreach ($f in Get-ChildItem $stage -Recurse -File | Sort-Object FullName) {
    $lines += "{0}`t{1}`t{2}" -f $f.Length, (Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLower(), $f.FullName.Substring($rootLen)
}
[IO.File]::WriteAllLines($listFile, $lines, (New-Object Text.UTF8Encoding $false))
Remove-Item $zip -ErrorAction SilentlyContinue
# (entry names with "/": Linux extractors - the Steam Deck's Ark - need them)
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    $root = (Resolve-Path $stage).Path.TrimEnd('\') + '\'
    foreach ($f in Get-ChildItem $stage -Recurse -File) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $f.FullName, $f.FullName.Substring($root.Length).Replace('\', '/'),
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
"Packaged $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB, $($lines.Count - 1) files)"
