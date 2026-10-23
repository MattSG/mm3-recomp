# Application-owned shared-read compatibility

Fork 95a3ec5694b6e19bef64dab0d850da19bf56afe7 removes unconditional broadened sharing from both native
NtCreateFile backends. Generic defaults now preserve caller ShareAccess.
xbox_SetReadOnlyFileSharing(BOOLEAN) configures an optional host compatibility
mechanism before file operations/workers begin. It broadens read sharing only
for read-only effective access; writes and incompatible existing handles are
still protected. Configuration is startup-only, not synchronized live state.

MM3 src/main.c explicitly enables the mode to preserve its existing multi-
instance asset-sharing behavior. Historical rationale lives in
legacy_file_shared_read_policy.md. No title check or game default is embedded
in the generic implementation, and no one-off callback framework was needed.

Actual runtime sharing fixture fails exclusive-read behavior under the old
unconditional policy. Final fixture checks default strict reads, opt-in shared
reads, writer rejection, and strict reads after clearing the option. Sharing
and diagnostics pass 2/2 Windows; sharing passes 1/1 Linux. Both game builds
pass with MM3 configuring the API. No live multi-instance gameplay is claimed.

Review scope extends through the Win32 create-file path and POSIX access/share
mapping excerpts, not the full file implementation. Directory-to-parent device
redirection, path/count/async semantics and other file-service gaps remain open.
