# Installer implementation

Complete code: `installer/installer.iss`, `installer/setup_helper.cpp`, `src/common.hpp`.
Inno stages the helper/payload under Program Files/W3UnicodeSaveFix and creates
normal uninstall registration. Detection uses a temporary helper. No Documents
redirection or game termination occurs; install/uninstall require a closed game.

Detection reads Steam registry/library manifests and GOG registry paths, validates
AMD64 bin/x64_dx12/witcher3.exe and allows Browse. Desktop/elevated SIDs must match.

Before game-file changes or save migration, a full original backup is checked by
relative file inventory, count, size and SHA-256, with an unchanged-source check.
The backup contains original-saves, a manifest and recovery state. Existing target
files are separately copied and verified in ascii-before. Empty sources are allowed
and no historical directory search occurs. Only sav/png/json migrate; differing
existing files abort rather than being overwritten. Originals remain.

New target directories grant access to the user, SYSTEM and Administrators. Existing
ACLs are not changed. Reparse paths are refused. A CREATE_NEW temporary write probe
deletes only itself. Game files added: ASI, INI and the pinned dinput8.dll if absent.
Unknown proxy DLLs block installation. No game executable is replaced.

Recovery state is written before game binaries. Copy failures can leave verified
save copies and backups; only newly added matching mod binaries are rolled back.
This is not a guaranteed all-or-nothing transaction.

Uninstall offers restoration to the recorded original directory, first backing it
up again. Missing files are copied; newer differing files require individual Yes
confirmation, staged/hash-checked replacement and another original-hash check.
Older conflicts stay. Only unchanged owned mod files are removed. An owned loader
is removed only if unchanged and no other ASI/plugin/update folders are detected.
Modified files, prior loaders, logs, backups and ASCII saves remain.

## Network and provenance

The end-user installer downloads nothing. Build-time `tools/get-dependencies.ps1`
fetches official assets pinned in dependencies.lock.json:

- https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip
- https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe

GitHub may redirect to release-asset storage. SHA-256 is checked before extraction
or execution. Inno's signature/publisher are checked too; the extracted loader has
an independent pinned digest and AMD64 check. Compiler files are not distributed
to players. The x64 loader is included with its MIT license.
