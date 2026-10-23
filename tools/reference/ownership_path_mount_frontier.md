# Path mount ownership frontier

Inspected fork 9613aea src/kernel/kernel_path.c in full, its public declarations,
and the production-backend save-path fixture. This records the initial ownership
finding and subsequent mount-policy migration, not a full filesystem audit.

## Confirmed policy

s_rules maps C:, Y:, their namespace forms, and paths below
\Device\Harddisk0\Partition2 to game_dir. The comments justify these aliases
with dashboard asset placement. Partition2's bare device instead maps to a
SystemData image, while Partition4 (the physical Y cache partition described
by the same source) maps to save_dir/Cache. The mapping of extracted application
assets is distinct from translating the console device namespace.

A Windows probe includes the actual kernel_save_paths fixture and production
kernel_path.c. It sets private in-memory roots without path initialization or
filesystem writes, then verifies C:\asset.bin, Y:\asset.bin and
\Device\Harddisk0\Partition2\asset.bin all resolve below the game root.
Probe source: conformance_tmp/ownership-path-probe/probe.c. Config/build logs:
conformance_tmp/ownership-path-probe-{config,build}.log. Output:

    C, Y and Partition2 asset paths select game root

resolve_symlink checks caller-created drive links before static rules, but
accepts only targets already covered by those rules. g_xbox_path_hook is an
observer called on matched paths; it cannot select or reject a translation.
It also does not see bare partition images or unmatched paths. Thus neither
existing interface supplies arbitrary host mount policy.

## Required migration

Separate extracted-asset/system mount selection from generic device translation.
Use a minimal startup-configured host mount mechanism if existing guest symbolic
links cannot express the required host roots. Put application layout selection
in the consuming recompilation repository. Preserve explicit save-root isolation,
case-insensitive lookup, character conversion, checked path construction and
bare-device image handling as shared mechanisms. Do not replace these with a
title-name conditional or make the observer ambiguously control translation.

Before changing defaults, inspect every consumer and fixture for dependency on
legacy C/Y/system aliases, including the non-MM3 dashboard use case. Verify
unconfigured behavior, explicit mounts, guest remount precedence, both path
backends and isolated save tests. MM3 must select only layouts it actually uses;
no blanket opt-in to historical dashboard aliases without evidence.

## Additional audit boundaries

Partition image creation/sizing and synthesized retail geometry, fallback game
root differences between Windows/POSIX, unchecked/truncated path conversions,
and namespace containment remain review items. Reading this source does not
establish safe arbitrary-input translation or full disk/filesystem conformance.
Historical title comments include DSTEAL_JP, Wreckless, Burnout and Halo traces;
those belong in the consuming repository when the executable policy migration
is complete. No path behavior changed in this checkpoint.

## Implemented separation

The shared API now accepts a borrowed, startup/quiesced XboxPathMount table:
case-insensitive guest prefix to UTF-8 host root, longest prefix first, with
validated structural input and explicit clearing. Guest-created drive links
resolve before mounts; custom device targets are supported. Bare partition
devices retain their images, including trailing-separator opens. Translation
checks output capacity and keeps a mounted volume root intact.

The generic fallback routes C:/Partition2 files to save_dir/SystemData and Y:
to save_dir/Cache. Applications select extracted dashboard layout explicitly;
legacy_path_layout_notes.md preserves the former title commentary and an
application-owned example table. MM3 has not enabled unused dashboard aliases.
Full path containment, disk geometry/image policy and platform fallback-root
differences remain separate audit items; this does not close those questions.

Verification: Windows kernel suite 62/62 and focused mount/save fixture 1/1;
Linux kernel suite 61/61 plus focused mount/save fixture. Tests cover explicit
legacy layout, system/cache defaults, longest-prefix boundaries, guest remount
precedence including a custom device, invalid table rejection without mutation,
clearing, small output rejection, bare devices and volume-root preservation.
The Windows volume-root check failed before its fix (C: instead of C:\).
Both final game builds pass. Final owned Vulkan run-7008on3d passes 49 steps
in 123.422 seconds, with 1.33619 metres of movement from teleport and inspected
night-driving capture-004.bmp. Evidence logs are conformance_tmp/ownership-mount-*
and conformance_tmp/game_test/run-7008on3d. Earlier run-nsz1qhft also passed,
but predates the volume-root preservation correction; final proof uses the
rebuilt executable. These checks do not prove a dashboard or arbitrary game.
