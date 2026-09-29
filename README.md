# W3 Unicode Save Fix

W3UnicodeSaveFix is a temporary workaround for The Witcher 3 Remastered save failures caused by non-ASCII characters in Windows profile/save paths.

**Experimental source publication.** Runtime and installer version remains `0.1.0`;
`v1.0.0` is planned, not released. The original one-byte experiment worked, but the
generalized full-path patch has not passed end-to-end save/reload testing. The exact
engine defect has not been established. A successful build is not proof of safety.

[Nexus Mods](https://www.nexusmods.com/witcher3/mods/13037) ·
[Build instructions](BUILDING.md) · [Technical analysis](docs/TECHNICAL_ANALYSIS.md) ·
[Release readiness](docs/RELEASE_READINESS.md)

## What does this fix?

The mod attempts to replace a cached narrow-character save-directory path with a
short ASCII-only destination. It is **not an official CD PROJEKT RED patch**.
The Windows account is not renamed. Installation does not delete original saves.
Make an independent backup before use.

Explicit reproduced bug example, not hardcoded configuration:

```text
C:\Users\Miško
C:\Users\Miško\Documents\The Witcher 3\gamesaves\
```

Manual save, quicksave and autosave failed. Diagnostics observed successful save
directory access and a successful free-space query, but no `.sav` creation attempt
in the captured save attempt. In a separate authorized experiment, changing the
cached runtime path to an existing ASCII-only path restored saving immediately
without restarting the game. A manual save and companion files were created.

The Czech character **š** was directly tested in that experiment. Other non-ASCII
characters may be affected, but have not all been tested. Successful file creation
alone does not establish successful loading after a restart.

## Symptoms

- Manual save, quicksave and autosave report that saving failed.
- The save directory exists and can be writable from a terminal.
- A Windows profile or Documents path contains non-ASCII characters.

These symptoms can have other causes. This is not a general permissions, disk-space,
cloud-sync or corrupt-save repair tool.

## How it works

See [src/plugin.cpp](src/plugin.cpp) and [src/common.hpp](src/common.hpp).

1. A minimal DllMain starts a worker. It checks for `witcher3.exe` in `x64_dx12`
   and pins its own module against unloading.
2. It resolves actual Documents with SHGetKnownFolderPath, appends
   `The Witcher 3\gamesaves\`, and losslessly converts the full NUL-terminated path
   using GetACP by default. An independently verified code page may be set in INI.
3. It skips already-ASCII source paths. The destination is
   `<Windows drive>\W3Saves\<USER_ID>\`. USER_ID is the first 16 lowercase hex digits
   of SHA-256 of the current Windows SID's UTF-8 text, not a hardcoded username.
4. The destination must exist, be writable and be strictly shorter in encoded bytes.
   A CREATE_NEW temporary probe writes/flushes data and deletes only itself on close.
5. After a delay and visible-window check, VirtualQuery scans committed, private,
   readable/writable, non-executable regions, excluding guard pages and the plugin's
   pattern buffers. Matches include the exact path and its terminating NUL.
6. Two complete scans must find exactly one candidate at the same address.
   Zero matches retry within a timeout; ambiguity, read faults, instability or
   exhausted budgets refuse the patch. There is no hardcoded runtime memory address.
7. Region and original contents are checked again immediately before writing.
   The full original capacity is replaced by Target + NUL + zero padding.
   Complete readback is required before logging `YES; Validation: SUCCESS`.

It does not patch UTF-16 strings or arbitrary username occurrences. A unique string
match is a heuristic, not proof of the buffer's engine ownership.

## Installation

**Stable release is on hold.** These describe the current experimental installer.

1. Independently back up saves and close the game yourself.
2. Run `W3UnicodeSaveFix_Setup.exe` as the user who plays the game and approve UAC.
   Elevation under a different desktop user's account is refused.
3. Review the detected Steam/GOG DX12 path or select it with Browse.
4. Review Summary: game, original save path, target, backup and loader status.
5. Install. Players need no Python, Visual Studio or manual copying.

The helper backs up the whole original save tree, verifying the relative file set,
count, each size/SHA-256 and unchanged source. An existing ASCII target is backed
up too. Only `.sav`, `.png`, `.json` migrate. Identical target files stay; different
ones block installation. See [installer behavior](docs/INSTALLER.md).

**Saves outside current Documents are not discovered/imported.** This includes
earlier experiment and rescue directories. If current Documents has no saves,
installation will not restore the Load Game list from those other directories.

The installer bundles pinned x64 Ultimate ASI Loader under MIT. An identical loader
is reused; unknown proxy DLLs block installation and are never overwritten.
There are no installer downloads. Setup is unsigned. Advanced manual installation
is described in [MANUAL_INSTALL.md](docs/MANUAL_INSTALL.md).

## Uninstallation

Close the game and use Windows Installed Apps. The helper offers to restore
missing/newer saves to the original directory recorded at installation. It first
creates another verified backup. Different newer files require an individual
confirmation (default No); older conflicts are skipped.

Only unchanged owned mod files are removed. Modified files are preserved and
reported. An owned loader is removed only when still identical and no other
ASI/plugin/update folders are detected; pre-existing loaders stay.
**ASCII saves, backups and logs remain.** A changed ASI left behind may remain
active until reviewed. The original bug may return on the next game start.

## Save locations

| Purpose | Location |
|---|---|
| Original | Actual Documents + `The Witcher 3\gamesaves` |
| Patched target | `<Windows drive>\W3Saves\<16-hex SID hash>\` |
| Backup | `<Windows drive>\W3UnicodeSaveFixBackup\<timestamp>-<random>\` |
| Original backup | `original-saves` inside that backup |
| Previous ASCII files | `ascii-before` inside that backup, when applicable |
| Runtime log | Beside the ASI, or inside the target as a fallback |

The target is used only when the runtime patch succeeds. Already-ASCII Documents
remain unchanged. To restore manually, close the game, preserve current saves
separately, then copy chosen backup files to the recorded original directory.
Never blindly overwrite differing current files.

## Safety

The project's plugin has no network client or telemetry code. It modifies memory
only in its own host process after checking the host name/location. It does not
attach a debugger, inject into other processes or rename Windows accounts.
It does not write registry values. The helper reads Steam/GOG registry entries;
**Inno Setup creates normal application/uninstall registration**, so the entire
installer is not accurately described as making zero registry changes.

No data is uploaded by project code. Local logs contain paths, PID and per-run
addresses; installation state records SID/paths and manifests record filenames
and hashes. These can contain personal data: redact them before sharing.
Third-party loader behavior is a separate trust boundary, documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The full-string rewrite is **not atomic**. Another thread may read, free or reuse
the buffer after validation. Preflight failures make no memory change; a fault or
race after writing starts has no guaranteed safe rollback. SEH handles access
faults, not semantic corruption. Indeterminate writes log UNKNOWN and are not
blindly retried. `DryRun=1` performs candidate diagnostics without the patch.

## Building from source

See [BUILDING.md](BUILDING.md). The public tree contains all plugin and installer
sources. Downloaded dependencies, build products, saves and diagnostics are excluded.
Actions builds/tests and uploads artifacts; it never creates a release.

## Known limitations

- x64 DX12 only; other builds/locales are not broadly validated.
- GetACP is a default assumption, not an engine encoding query. Lossy conversions
  are refused; this is not universal Unicode support.
- Already-ASCII source skips patching even though Setup creates a target directory.
- No automatic import from historical/experimental save directories.
- Reparse-point/OneDrive paths are conservatively rejected by file operations.
- Window/delay readiness is heuristic, not an engine initialization callback.
- Cross-region strings and ambiguous candidates are unsupported.
- Cloud synchronization of the target is not configured or guaranteed.
- The full-path rewrite differs from the successful one-byte experiment.
- Installer failure recovery, uninstall and fresh-launch save loading need further
  validation: [release gates](docs/RELEASE_READINESS.md).

## Troubleshooting

- **Missing Load Game:** compare current Documents, logged target and older rescue
  directories. Missing import is not evidence of deletion. Preserve every copy.
- **`source is already ASCII`:** expected no-op; routing was not changed.
- **No log:** investigate loader compatibility/permissions; do not assume success.
- **No/ambiguous candidate:** no patch. Check path/code page; never reuse an old
  runtime address or force a guessed match.
- **UNKNOWN:** a raced write has no guaranteed rollback. Preserve existing saves
  and report a redacted log; do not rely on the session's save state.

## Technical details

[Technical analysis](docs/TECHNICAL_ANALYSIS.md) separates evidence and hypothesis.
[Installer behavior](docs/INSTALLER.md) documents copies, backups and uninstall.
[Source review](docs/SECURITY_REVIEW.md) maps trust claims to code.

## License

Existing [MIT LICENSE](LICENSE), including the Ultimate ASI Loader MIT notice.
See [third-party notices](THIRD_PARTY_NOTICES.md) for dependency provenance and
build-tool terms. Not affiliated with CD PROJEKT RED.
