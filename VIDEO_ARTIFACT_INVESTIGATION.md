# MM3 video artifact checkpoint

Date: 2026-10-04

## Windows regeneration and runtime

- Regenerated all game C from `game_files/default.xbe` with XboxRecomp commit `ffbf50d` and the current manual-function list: 26,409 of 26,446 functions translated, zero translation failures.
- Restored the project-owned `recomp_types.h` and `recomp_cpu.h` after generation, then built `mm3_recomp` with Visual Studio 18 2026. Executable SHA-256: `36DBDC081D8D3A6F6F6C5F654C3AFC65C90EA23F327705ED021464BA5E8816C4`.
- A 120-second Windows run timed out without a crash. It opened `dice.bik`, `msgs.bik`, and `intro.bik`; the framebuffer window stayed at `FPS: 0.0 | draws: 0`. There were no `[VIDEO]` messages from the Media Foundation player, so this run followed the guest Bink decoder path.

## Decoder buffer captures

- Captures were taken after `sub_002F9E50` returned to `sub_002FBC10` (guest return address `0x002FBD6C`). They are intermediate guest luma-buffer snapshots; they are not evidence that a complete frame was presented.
- The PTS 23 packet prefix maps to `msgs.bik` offset `1,032,964`. Its captured luma buffer is byte-for-byte identical to the earlier baseline capture.
- The PTS 24 packet prefix maps to offset `1,041,476`. Its full-regeneration capture is byte-for-byte identical to the earlier capture made when only `sub_002F9170` had been regenerated. Against FFmpeg's PTS 24 luma reference, the captured buffer has mean absolute error `26.3923` and visibly contains block corruption.
- The full regeneration applies the committed x86 five-bit shift-count masking throughout `sub_002F9E50`. It does not change either captured output above, so this shift fix is not the cause of these observed artifacts.

## Next diagnostic boundary

Trace the guest Bink decoder through completion of the full frame, then compare the finalized Y plane and presentation buffer against the same decoded Bink frame. Continue from the guest decoder and its memory/write bounds; the Media Foundation pump was not active in this run.

## Decoder return-boundary capture

- Captured the 307,200-byte Y buffer at `sub_002F9E50` entry and after returns through `sub_002F9E50`, `sub_002FBC10`, and `sub_002F5E20` for the PTS 24 packet.
- All four snapshots are byte-identical (SHA-256 `33771b78ae50b54d445054ed870e9723689151762d3e13eaf1439477647287bd`). The scattered block errors are already present at helper entry; the outer decode returns through `sub_002F5E20` do not alter this Y buffer.
- Compared with the FFmpeg reference PGM (`640x480`), the captured image has MAE `26.3923`, with 302,907 of 307,200 bytes differing. The scene is recognizable, but block-shaped errors remain. The next investigation should focus inside `sub_002F9E50` or on its inputs/state, including prior-frame state for delta blocks.

## Original Xbox comparison

- xemu reached XBE entry `0x00083C55`. Its `sub_002F9E50` hit used an 8 KiB input capture byte-for-byte equal to the same PTS 24 prefix from `msgs.bik` as the Windows recomp capture.
- At the matching final helper call (return address `0x002FBD6C`, mode `0x4000`), the original and recomp scalar arguments matched. The original output buffer was already a clean frame before this call; its entry and return snapshots are identical. The host output buffer was already corrupted before the same call; its entry and return snapshots are also identical.
- Against FFmpeg's PTS 24 luma reference, xemu's frame MAE is `11.7564`; the host recomp's is `26.3923`. The host image is visibly more corrupted than the original. This rules out the final helper call and presentation as the first divergence: the next comparison needs the earlier `sub_002F9E50` calls within `sub_002FBC10`.
- The seeded generation added `sub_002F8E90` to C and the dispatch table, but it did not change this PTS 24 capture. A 60-second run through `dice.bik`, `msgs.bik`, and `intro.bik` did not hit that callback. Its absence is not yet a demonstrated cause for this packet path.

XboxRecomp issue [#127](https://github.com/sp00nznet/xboxrecomp/issues/127) reports a Black recompilation stalled before its first valid D3D Present. It is useful context for the zero-draw frontier, but does not establish a cause for MM3's Bink pixel corruption.
