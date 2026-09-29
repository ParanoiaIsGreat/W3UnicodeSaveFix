# Advanced manual installation (experimental)

The Setup executable is preferred because it performs verified backups, migration,
loader checks and records uninstall state. This manual archive alone does not
perform those operations. Do not place its contents over unknown existing files.

1. Close the game yourself. Make an independent backup of current Documents saves
   and any historical experiment/rescue directories. Verify each copied file's
   size and SHA-256 with Get-FileHash before proceeding.
2. Calculate the same per-user target as the plugin using PowerShell:

```powershell
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$sha = [Security.Cryptography.SHA256]::Create()
try { $id = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($sid))).Replace('-', '').ToLower().Substring(0,16) }
finally { $sha.Dispose() }
$target = Join-Path ([IO.Path]::GetPathRoot([Environment]::GetFolderPath('Windows'))) ('W3Saves\' + $id)
$source = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'The Witcher 3\gamesaves'
$source
$target
```

3. Create that target directory with write access for your normal Windows account.
   Preserve any existing target files. Copy desired sav/png/json files without
   replacing differing files; verify their hashes. Never delete the source.
4. Review dinput8.dll: only the bundled exact x64 loader is validated. Stop if an
   unknown loader/proxy DLL already exists. Do not replace it blindly.
5. Copy the ASI/INI (and loader only if absent) to bin/x64_dx12. Set DryRun=1 first.
   On a future game launch inspect the log. DryRun does not patch memory; after a
   reviewed candidate check, enabling the patch requires DryRun=0 and a later launch.
   Already-ASCII Documents always skip patching, regardless of destination contents.

Manual installation does not register an uninstaller. To remove it, close the game,
back up all saves, optionally copy chosen newer target saves back with conflict
review, and remove only the files you added. Retain a loader used by other mods.
Never delete the target or backups as part of removal. These instructions do not
establish tested compatibility or eliminate the concurrency risks in README.
