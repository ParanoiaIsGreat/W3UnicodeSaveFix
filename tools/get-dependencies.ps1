# Build-time downloads only. Nothing is copied to a game installation.
param([string]$CacheDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lock=Get-Content -LiteralPath (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
if(-not $CacheDirectory){$CacheDirectory=Join-Path $root 'build\dependencies'}
$cache=[IO.Path]::GetFullPath($CacheDirectory)
New-Item -ItemType Directory -Path $cache -Force | Out-Null
function VerifiedDownload($url,$path,$digest){
 if(-not(Test-Path -LiteralPath $path)){Invoke-WebRequest -Uri $url -OutFile $path}
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $digest){throw "Dependency hash mismatch: $path (retained for inspection)"}
}
$archive=Join-Path $cache 'Ultimate-ASI-Loader-NoPDB_x64.zip'
VerifiedDownload $lock.ultimateAsiLoader.url $archive $lock.ultimateAsiLoader.archiveSha256
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try {
 $entries=@($zip.Entries | Where-Object {$_.Name -eq 'dinput8.dll'})
 if($entries.Count -ne 1){throw 'Expected exactly one dinput8.dll in official archive'}
 $loader=Join-Path $cache 'dinput8.dll'
 if(-not(Test-Path -LiteralPath $loader)){[IO.Compression.ZipFileExtensions]::ExtractToFile($entries[0],$loader,$false)}
} finally {$zip.Dispose()}
if((Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash -ne $lock.ultimateAsiLoader.dllSha256){throw 'Loader DLL digest mismatch'}
$bytes=[IO.File]::ReadAllBytes($loader)
$pe=[BitConverter]::ToInt32($bytes,60)
if([BitConverter]::ToUInt16($bytes,0) -ne 0x5A4D -or $pe -lt 64 -or $pe+6 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664){throw 'Loader is not PE AMD64'}
$innoSetup=Join-Path $cache 'innosetup-6.7.3.exe'
VerifiedDownload $lock.innoSetup.url $innoSetup $lock.innoSetup.sha256
$signature=Get-AuthenticodeSignature -LiteralPath $innoSetup
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notlike '*Pyrsys B.V.*'){throw 'Inno signature/publisher verification failed'}
$inno=Join-Path $cache 'inno'
if(-not(Test-Path -LiteralPath (Join-Path $inno 'ISCC.exe'))){
 $arguments='/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PORTABLE=1 /DIR="'+$inno+'"'
 $process=Start-Process -FilePath $innoSetup -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
 if($process.ExitCode -ne 0){throw "Portable Inno extraction failed: $($process.ExitCode)"}
}
if(-not(Test-Path -LiteralPath (Join-Path $inno 'ISCC.exe'))){throw 'ISCC.exe missing after portable extraction'}
[pscustomobject]@{LoaderDll=$loader;InnoCompiler=(Join-Path $inno 'ISCC.exe')}
