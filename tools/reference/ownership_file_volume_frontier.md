# File-volume query ownership frontier

Scoped kernel_file.c review at fork 852fedd7ebc43c89af9effe474bd1853de868e01: shared default volume
geometry, observer/header sections, and both native volume-size query bodies.
The entire 1969-line file was not reviewed. Existing device-path translation
belongs to kernel_path.c, while native volume queries retain generic services.

Captured Half-Life 2 startup notes moved to legacy_file_volume_observations.md.
The retained 512-byte sector/16 KB allocation unit is now described as an
emulated default rather than proof about every Xbox device/FATX volume.

Both backends ignored output Length and wrote 24 bytes for any non-null buffer.
The size-information branch now rejects every undersized length before touching
the output, reports STATUS_INFO_LENGTH_MISMATCH and zero returned bytes.
A direct actual-runtime fixture fails status/no-write checks for lengths 0-23
before the fix, then passes those checks plus exact-size/canary checks on
Windows and Linux. All 46 Windows bridge tests pass; both game builds pass.
The guest bridge calls this shared export after its own buffer validation.

Remaining issues: modeled geometry is fixed, Win32 capacity is queried for the
current host volume rather than the handle, host-query failures return invented
capacity, and other information classes are unsupported. Wider file ABI,
counted paths, async behavior and remaining title narratives need review.
Source remains needs_generic_fix; no full file-service or volume-fidelity claim.
