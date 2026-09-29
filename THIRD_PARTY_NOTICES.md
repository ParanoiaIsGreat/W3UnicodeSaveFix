# Third-party notices

## Ultimate ASI Loader 9.7.4, NoPDB x64

Author/copyright: (c) 2023 ThirteenAG. License: MIT, reproduced in LICENSE.
Redistribution of the unchanged loader with its notice is permitted.

- Project: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- License for this tag: https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/v9.7.4/license
- Asset: https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip
- Archive SHA-256: `e5860e7d9a1805267535b65749575b5e406cc6ea3325c7392189c578815045d1`
- DLL SHA-256: `031a3e5576d91dce1e438d36b9a3d462c7334ab4791990a8ff1e3ddc0e132daf`

The loader is downloaded only at build time, ignored in Git and bundled in setup
and manual distribution archives. Its digest and x64 architecture are checked.
This is provenance/hash verification, not a claim of Authenticode signing or a
complete security audit. First-party no-network claims do not audit third-party code.

## Inno Setup 6.7.3

Copyright (C) 1997-2026 Jordan Russell; portions (C) 2000-2026 Martijn Laan.
https://jrsoftware.org/ — https://github.com/jrsoftware/issrc/tree/is-6_7_3

Official license: https://github.com/jrsoftware/issrc/blob/is-6_7_3/license.txt
It permits use and redistribution subject to retaining notices, not misrepresenting
origin and identifying modified versions. This project uses the unchanged compiler
and generated setup engine with their existing copyright/site notices. The compiler
itself is not shipped to players. Its verified download is hash-pinned and signed
by Pyrsys B.V. No project signature or third-party certificate is applied to Setup.

## Microsoft build tools

MSVC and Windows SDK are build prerequisites under Microsoft's terms, not copied
into this repository or distributed as build tools. The first-party executables use
the static MSVC runtime. Maintainers must use a properly licensed toolchain and
follow its redistributable-code terms; the project's MIT grant does not relicense it.
