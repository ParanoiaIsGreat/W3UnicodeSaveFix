param([string]$InnoCompiler='',[string]$LoaderDll='',
 [string]$MsvcVersion='14.44.35207',[string]$WindowsSdkVersion='10.0.26100.0',
 [switch]$SkipTests)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$build=Join-Path $root 'build'
$dist=Join-Path $root 'dist'
New-Item -ItemType Directory -Path $build,$dist,(Join-Path $root 'vendor') -Force | Out-Null
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if(-not(Test-Path -LiteralPath $vswhere)){throw 'Install Visual Studio Build Tools with Desktop development with C++'}
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Get-Item -LiteralPath (Join-Path $vs ('VC\Tools\MSVC\'+$MsvcVersion))
$sdkBase='C:\Program Files (x86)\Windows Kits\10'
$sdk=Get-Item -LiteralPath (Join-Path $sdkBase ('Include\'+$WindowsSdkVersion))
if(-not $vc -or -not $sdk){throw 'Missing x64 MSVC/Windows SDK'}
$toolbin=Join-Path $vc.FullName 'bin\Hostx64\x64'
$compiler=Join-Path $toolbin 'cl.exe'
$oldPath=$env:PATH;$env:PATH=$toolbin+';'+$oldPath
$includes=@(('/I'+(Join-Path $vc.FullName 'include')),('/I'+(Join-Path $sdk.FullName 'ucrt')),('/I'+(Join-Path $sdk.FullName 'um')),('/I'+(Join-Path $sdk.FullName 'shared')))
$libs=@(('/LIBPATH:'+(Join-Path $vc.FullName 'lib\x64')),('/LIBPATH:'+(Join-Path $sdkBase ('Lib\'+$sdk.Name+'\um\x64'))),('/LIBPATH:'+(Join-Path $sdkBase ('Lib\'+$sdk.Name+'\ucrt\x64'))))
$common=@('/nologo','/std:c++17','/W4','/O2','/MT','/EHsc','/utf-8','/DUNICODE','/D_UNICODE','/guard:cf','/Brepro')+$includes
$link=@('/MACHINE:X64','/DYNAMICBASE','/HIGHENTROPYVA','/NXCOMPAT','/GUARD:CF','/Brepro')+$libs+@('kernel32.lib','user32.lib','shell32.lib','ole32.lib','advapi32.lib','bcrypt.lib')
function Compile([string]$Source,[string]$Output,[switch]$Dll){$args=$common+@((Join-Path $root $Source),('/Fo'+(Join-Path $build ([IO.Path]::GetFileNameWithoutExtension($Output)+'.obj'))),('/Fe'+$Output));if($Dll){$args+='/LD'};$args+='/link';$args+=$link;& $compiler @args;if($LASTEXITCODE -ne 0){throw "Compilation failed: $Source"}}
try{
 Compile 'src\plugin.cpp' (Join-Path $build 'W3UnicodeSaveFix.dll') -Dll
 Copy-Item -LiteralPath (Join-Path $build 'W3UnicodeSaveFix.dll') -Destination (Join-Path $dist 'W3UnicodeSaveFix.asi') -Force
 Compile 'installer\setup_helper.cpp' (Join-Path $build 'W3UnicodeSaveFixHelper.exe')
 if(-not $SkipTests){Compile 'src\tests.cpp' (Join-Path $build 'tests.exe');& (Join-Path $build 'tests.exe');if($LASTEXITCODE -ne 0){throw 'Plugin tests failed'};& (Join-Path $build 'W3UnicodeSaveFixHelper.exe') selftest (Join-Path $build ('fixture-'+[guid]::NewGuid().ToString('N')));if($LASTEXITCODE -ne 0){throw 'Helper tests failed'}}
 if($LoaderDll){Copy-Item -LiteralPath $LoaderDll -Destination (Join-Path $root 'vendor\dinput8.dll') -Force}
 $loader=Join-Path $root 'vendor\dinput8.dll'
 if(-not(Test-Path -LiteralPath $loader) -or (Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash -ne '031A3E5576D91DCE1E438D36B9A3D462C7334AB4791990A8FF1E3DDC0E132DAF'){throw 'Obtain pinned official loader from dependencies.lock.json; wrong or missing SHA-256'}
 foreach($name in @('W3UnicodeSaveFix.ini','README.md','LICENSE','THIRD_PARTY_NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $dist $name) -Force}
 if(-not $InnoCompiler){$InnoCompiler='C:\Program Files (x86)\Inno Setup 6\ISCC.exe'}
 if(-not(Test-Path -LiteralPath $InnoCompiler)){throw 'Specify -InnoCompiler path to Inno Setup 6.7.3 ISCC.exe'}
 & $InnoCompiler ('/DProjectRoot='+$root) (Join-Path $root 'installer\installer.iss');if($LASTEXITCODE -ne 0){throw 'Installer compilation failed'}
 & (Join-Path $root 'tools\package.ps1')
 [pscustomobject]@{MSVC=$vc.Name;WindowsSDK=$sdk.Name;InnoCompiler=$InnoCompiler;UTC=(Get-Date).ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'build-environment.json') -Encoding utf8
}finally{$env:PATH=$oldPath}
