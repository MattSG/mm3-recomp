# MM3 video artifact checkpoint

## Verified first-video decoder fix

- Compared the first 40 decoder outputs with original Xbox execution in xemu. All compressed-input prefixes matched. Frames 1-5 matched exactly; frame 6 first diverged in one 8x8 luma block at x=200-207, y=96-103. Subsequent frames accumulated further errors.
- A write watchpoint identified the scalar Bink IDCT at `sub_002FE550`. Original instruction `0x002FE5FC` is `movsx ebp,bp`; the lifter omitted BP/SP from its signed 16-bit register cases and emitted `ebp = LO16(ebp)`. Negative coefficients became large positive values.
- XboxRecomp commit `5d0b56be1a58d4cc8a58d8f34a0faf30f3898f2d` fixes generic MOVSX lifting for BP and SP. Full seeded C regeneration and a Windows MSVC build succeeded. Executable SHA-256: `E08E180C0483580B249011DBDC19E3FA6A631B8A9FCC6D168A2AD12463C37324`.
- After the fix, all 40 captured luma frames match xemu byte for byte (307,200 bytes each). Evidence is under ignored `conformance_tmp/bink_first_frames_20261004/`. This proves the initial luma corruption fix; complete colour-plane and playback verification across all three movies is still pending.

Date: 2026-10-04

## Colour planes and black-flash fix

- A complete-sequence comparison recorded 3,566 original decoder outputs. Matching compressed-data prefixes against rebuilt captures gave 3,565 byte-identical Y/U/V triples. One host capture was invalid: `gu` stopped at another `sub_002FBC10` breakpoint rather than its return, and its saved image matched the preceding original frame. Replaying that packet (`msgs.bik` frame index 153) separately produced identical complete compressed video data (17,296 bytes), previous YUV data (460,800 bytes), and final YUV data (460,800 bytes). Final replay hash: `F78FF10DB56538CB95EDF682168CB8566A3E68C4D3A98832F91B8735290D1C8E`.
- Capturing framebuffer refreshes during a bright scene found 9 completely black images out of 60. `fb_overlay` consumed the PVIDEO pending bit after one refresh; the next refresh rebuilt the black base framebuffer and no longer drew the video.
- Scanout now owns a copy of the accepted YUY2 submission and its registers, retaining it between submissions while allowing guest bank reuse. STOP (bit 0) or an invalid-size teardown clears the retained overlay. After rebuilding, the equivalent 60-refresh sample contained zero black images; every sample had at least 307,037 nonblack pixels.
- The xemu [PVIDEO register implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pvideo.c) clears the overlay at STOP, and its [display implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/display.c) composites an enabled overlay on successive display refreshes. This supports retaining displayed content separately from consumed pending work in this runtime.
- Evidence: ignored `conformance_tmp/bink_all_movies_movsx_20261004/` and `conformance_tmp/pvideo_hold_20261004/`. Latest executable SHA-256: `53E69BDA9BB1F102F4E0E1874F33194D5198A3A70E1CFA928629DB9A7B24A9B5`.

## Playback acceptance

- A 130-second normal Windows run opened all three movies and produced framebuffer captures of the DICE logo, Microsoft intro, and main intro scenes without the former block corruption. No crash was reported. Captures include `normal1/frame_015.bmp`, `frame_022.bmp`, and `frame_125.bmp` under the evidence directory above.
- A second cold run used `run_clean_strict.ps1` with framebuffer sampling disabled. The user confirmed: "Video looks good, just a bit laggy", followed by "Lag is done goal complete". The remaining validation process was stopped after acceptance.
- Video milestone commits: outer `515ddaf` (signed IDCT coefficients) and `99cb046` (persistent overlay); nested XboxRecomp `5d0b56b` and `fe42f62`. Audio is outside this verification.

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
