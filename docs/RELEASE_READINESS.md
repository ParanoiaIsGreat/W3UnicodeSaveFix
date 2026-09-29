# Release readiness

**Hold v1.0.0 tag and GitHub Release.** Source publication is not a stable-release
claim. Current runtime/setup identity stays 0.1.0 experimental.

Completed: native/Inno builds; synthetic exact/ambiguous NUL matches, full scans,
chunk boundaries, excluded/read-only/image/mapped/guard memory, stale content,
short replacement/zero padding, read faults, encoding refusal; helper backup and
no-overwrite fixtures. A real loader run produced the expected already-ASCII no-op.
The older one-byte CP1250 experiment created a manual save.

Required before a stable release:

- Full-path patch, manual/quick/auto save and successful loading after fresh launch.
- Verified game code pages and more locales/builds.
- Installer wizard, failures/cancel, uninstall and restore on isolated Steam/GOG PCs.
- Historical/rescue save import and clear empty-source handling. Current code does
  not discover these files, as demonstrated by the missing Load Game follow-up.
- Clear ASCII no-op UX: Setup creates a target although runtime skips that source.
- Permissions, reparse/cloud paths, full disks, changing files and existing loaders.
- Hosted Actions execution; local success alone is not a hosted result.

The build workflow never publishes a release. Before the planned v1.0.0 tag, align
source/setup versions and rebuild/retest all assets. Planned title:
`W3 Unicode Save Fix v1.0.0`.

Draft release description:

> An unofficial temporary workaround for Witcher 3 save failures involving
> non-ASCII save paths. Dynamically locates the expected cached path and redirects
> it to a short ASCII destination after validation. Includes backup-aware setup and
> local logging. Back up saves first and review encoding/concurrency limitations.

Planned assets: Setup.exe, Manual.zip, SHA256SUMS.txt. No saves, logs, traces, dumps
or local installation state may be included.
