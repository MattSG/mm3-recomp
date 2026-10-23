# Multi-instance read sharing policy moved to MM3

The old runtime always broadened read-only host opens, ignoring a caller's
exclusive ShareAccess. MM3 main now explicitly enables this compatibility mode
through xbox_SetReadOnlyFileSharing. The generic runtime defaults to preserving
caller flags on Windows and POSIX; applications configure the option before
file operations or worker startup. The setting does not override write access
or previously opened incompatible host handles.

Former rationale:

```text
    /* The Xbox has one process, so a title's exclusive share mode only ever
     * meant "not within this title". On the host a second copy of the game
     * (system-link testing) reads the same disc files: read-only opens share
     * reads, or the first copy locks the second out of D:. */
    /* Win32 share rules, as in the Win32 backend: the caller's ShareAccess, and
     * read sharing for read-only opens (system-link testing shares read-only
     * disc files between two copies of the game). */
```
