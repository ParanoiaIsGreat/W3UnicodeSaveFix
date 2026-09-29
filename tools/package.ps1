# Deterministic entry ordering/timestamps. Creates distribution assets, never installs.
param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $root 'dist'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$items=[ordered]@{
 'W3UnicodeSaveFix.asi'=(Join-Path $dist 'W3UnicodeSaveFix.asi')
 'W3UnicodeSaveFix.ini'=(Join-Path $root 'W3UnicodeSaveFix.ini')
 'dinput8.dll'=(Join-Path $root 'vendor\dinput8.dll')
 'README.md'=(Join-Path $root 'README.md')
 'MANUAL_INSTALL.md'=(Join-Path $root 'docs\MANUAL_INSTALL.md')
 'BUILDING.md'=(Join-Path $root 'BUILDING.md')
 'THIRD_PARTY_NOTICES.md'=(Join-Path $root 'THIRD_PARTY_NOTICES.md')
 'LICENSE'=(Join-Path $root 'LICENSE')
}
# Include linked docs so README links also work in the manual package.
Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter '*.md' -File | ForEach-Object {$items['docs/'+$_.Name]=$_.FullName}
$path=Join-Path $dist 'W3UnicodeSaveFix_Manual.zip'
$file=[IO.File]::Open($path,[IO.FileMode]::Create,[IO.FileAccess]::Write)
try {
 $zip=[IO.Compression.ZipArchive]::new($file,[IO.Compression.ZipArchiveMode]::Create,$true)
 try {foreach($name in @($items.Keys | Sort-Object)){
  $entry=$zip.CreateEntry($name,[IO.Compression.CompressionLevel]::Optimal)
  $entry.LastWriteTime=[DateTimeOffset]::new(2026,1,1,0,0,0,[TimeSpan]::Zero)
  $out=$entry.Open();$inputFile=[IO.File]::OpenRead($items[$name])
  try{$inputFile.CopyTo($out)}finally{$inputFile.Dispose();$out.Dispose()}
 }}finally{$zip.Dispose()}
}finally{$file.Dispose()}
$names=@('W3UnicodeSaveFix_Setup.exe','W3UnicodeSaveFix_Manual.zip','W3UnicodeSaveFix.asi','W3UnicodeSaveFix.ini','README.md','LICENSE','THIRD_PARTY_NOTICES.md')
$lines=foreach($name in $names | Sort-Object){((Get-FileHash -LiteralPath (Join-Path $dist $name) -Algorithm SHA256).Hash.ToLower())+'  '+$name}
[IO.File]::WriteAllText((Join-Path $dist 'SHA256SUMS.txt'),(($lines -join "`n")+"`n"),[Text.UTF8Encoding]::new($false))
