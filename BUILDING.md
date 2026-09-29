# Building and verifying

## Maintainer requirements

- Windows x64, PowerShell **7** and Git.
- Visual Studio 2022 Community or Build Tools with Desktop development with C++.
- Reference toolset MSVC **14.44.35207**, Windows SDK **10.0.26100.0**.
- Inno Setup **6.7.3**, obtained from the pinned official release.
- Pinned Ultimate ASI Loader **9.7.4 NoPDB x64**.

No game installation is needed to compile or run the synthetic tests. These tools
are not required on player machines. MSVC/SDK are Microsoft-licensed build tools,
not proprietary project-local dependencies. The plugin/helper link the static CRT.

## Clean clone build

```powershell
git clone https://github.com/kubosmiki-coder/W3UnicodeSaveFix.git
Set-Location W3UnicodeSaveFix
$deps = & .\tools\get-dependencies.ps1
.\build.ps1 -InnoCompiler $deps.InnoCompiler -LoaderDll $deps.LoaderDll
```

The dependency script downloads into ignored `build/dependencies`, verifies SHA-256,
checks Inno Authenticode/publisher, and extracts a portable compiler there. It does
not install the mod, start the game, or alter Windows Documents. See the exact
official URLs/digests in `dependencies.lock.json`. A failed verification stops.

For already verified offline dependencies:

```powershell
.\build.ps1 -InnoCompiler 'C:\BuildTools\Inno\ISCC.exe' -LoaderDll 'C:\BuildTools\UAL\dinput8.dll'
```

The loader digest is always checked by the build. With an externally supplied Inno
path, the maintainer is responsible for verifying the compiler distribution.

The build uses x64 MSVC C++17 `/O2 /MT /W4 /guard:cf /Brepro`, not Debug. The linker
enables ASLR, high-entropy VA and NX. It produces a DLL and copies it as
`dist/W3UnicodeSaveFix.asi`, builds the native installer helper, runs synthetic
memory/filesystem tests, then invokes Inno on `installer/installer.iss`.

Output: Setup.exe, ASI, INI, Manual.zip, README, LICENSE, third-party notices and
SHA256SUMS.txt in `dist`. They are ignored by Git; publish approved assets as Releases.
The setup remains 0.1.0 experimental until the documented v1.0.0 release gates pass.

To rebuild only the installer after a successful native build:

```powershell
$repo = (Get-Location).Path
& $deps.InnoCompiler ("/DProjectRoot=" + $repo) .\installer\installer.iss
if ($LASTEXITCODE -ne 0) { throw 'Installer build failed' }
.\tools\package.ps1
```

## Other installed toolchain versions

Use explicit `-MsvcVersion` and `-WindowsSdkVersion` values if necessary. Changing
tool versions can change binaries. The reference version is the default rather
than silently selecting a newer toolset. VS installation is discovered with vswhere.
`build/build-environment.json` is local/ignored and may contain a local compiler path.

## Verify source-to-binary correspondence

Check out the exact reviewed commit/tag, use the reference toolchain and pinned
dependencies, then build in a second clean clone. Compare complete SHA-256 values:

```powershell
Get-Content .\dist\SHA256SUMS.txt | ForEach-Object {
    $hash, $name = $_ -split '  ', 2
    if ((Get-FileHash -LiteralPath (Join-Path '.\dist' $name) -Algorithm SHA256).Hash -ne $hash) {
        throw "Hash mismatch: $name"
    }
}
```

For a published release, compare against a separately obtained trusted release
manifest, not just the freshly generated local manifest. Hash equality proves byte
equality, not safety or trustworthy provenance by itself. No signing key is used.

Git uses LF normalization, Inno entries use `notimestamp`, MSVC uses `/Brepro`, and
manual ZIP entries have fixed timestamps and ordering. Compare with the same
PowerShell/.NET compressor version for byte-identical ZIP output. Different compiler,
SDK, Inno or compressor versions are not expected to reproduce identical bytes.
The original project reproduced across two directories; see the preparation report
for the independent clean-source rebuild of this reorganized tree.

## GitHub Actions

`.github/workflows/build.yml` runs on `windows-2022`, discovers installed MSVC/SDK,
fetches hash-pinned dependencies, builds/tests and uploads only distribution files.
It uses no user-configured secrets; default GitHub artifact services use their normal
job credentials. Permissions are read-only and checkout does not persist credentials.
It cannot create a tag or release. Hosted toolchain updates mean it is a portability
build, not necessarily a byte-identical reference build. Workflow execution must be
observed on GitHub before claiming it passed there.
