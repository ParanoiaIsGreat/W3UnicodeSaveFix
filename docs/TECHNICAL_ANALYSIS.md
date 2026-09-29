# Technical analysis

## Reproduction and observations

Explicit bug example: `C:\Users\Miško`, with cached save directory
`C:\Users\Miško\Documents\The Witcher 3\gamesaves\`.
Manual save, quicksave and autosave failed. Process Monitor showed SUCCESS for
save-directory access and free-space query, but no `.sav` creation attempt in the
captured save attempt. Terminal write access alone did not explain the failure.
This does not prove that every possible filesystem operation was captured.

Read-only memory inspection found a writable ANSI Windows-1250 path and wide-string
data. In a separately authorized experiment, changing one byte from `š` (CP1250
0x9A) to `s` (0x73), with an existing ASCII destination, restored saving without a
restart. A 238535-byte manual `.sav` and `.png`/`.json` companions were created.
The save's integrity was checked against a retained hash; successful post-restart
loading and full format validation were not established.

No historical runtime address is published. ASLR and per-process allocations make
an old address unstable and unsuitable for another game session.

## Interpretation

Evidence supports a narrow-path handling/conversion/validation problem. The exact
engine routine and universal behavior for other characters are unproven. Only
Czech `š` was directly tested. Changing Documents while a game runs does not prove
it refreshes a cached path.

## Generalized implementation

The plugin uses actual current Documents and dynamic exact NUL-terminated matches
in private committed writable memory, not old addresses. Two stable unique scans
and revalidation precede a shorter whole-path replacement and zero filling.
Dedicated source/rollback buffers are excluded from scanning.

This full-string write is different from the original one-byte change. It is not
atomic; unique bytes do not prove engine ownership. SEH/readback cannot prevent
concurrent reads, frees or reuse. GetACP is an assumption, not engine detection.

## Observed installer follow-up

With Documents already redirected to an ASCII path, the installed plugin loaded
and logged `Patch applied: NO; source is already ASCII`. The current Documents
save directory contained cloud metadata but no saves; the new target was empty.
The earlier experimental manual save remained in a different ASCII directory,
outside the import source. Missing Load Game was consistent with missing migration,
not deletion. Current Setup imports only current Documents. This is a release gate,
not evidence of successful end-to-end runtime patching.

## References

- [VirtualQuery](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery)
- [DllMain practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
