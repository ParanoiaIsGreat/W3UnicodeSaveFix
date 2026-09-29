# Source trust review

Scope: first-party source/build configuration, not a formal third-party audit.

| Claim | Evidence and qualification |
|---|---|
| No fixed game address | VirtualQuery and dynamic exact path scans in plugin.cpp. |
| No remote patch | Direct pointers in the host; no debugger/injection/remote-write APIs. |
| No runtime network/telemetry | No network client in plugin/common/helper. Build downloader intentionally uses GitHub. Loader is a separate trust boundary. |
| No account rename | SID is read/hashed; no account-changing APIs. |
| Registry | Plugin does not write it; helper reads Steam/GOG; Inno creates normal uninstall registration. |
| Local personal data | Logs contain paths/PID/addresses; state contains SID/paths; manifests contain filenames/hashes. No upload. |
| Save preservation | CopyFileW refuses overwrite; install conflicts abort; restore overwrite needs confirmation and backup. |
| Protection changes | Runtime patch does not change game-page protections. Tests use VirtualProtect; static CRT imports can contain that API too. |

Validation does not synchronize with the engine. Free/reuse/races remain possible;
SEH cannot guarantee semantic correctness or rollback. Filesystem checks also are
not a security boundary against a malicious local process racing an elevated install.

Publication checks examine staged Git contents and reject binaries, saves, captures,
personal paths and common secret patterns. This is not a formal proof of absence
of secrets. Review every future diff as well as running automated checks.
