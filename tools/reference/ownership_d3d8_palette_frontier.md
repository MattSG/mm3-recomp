# D3D8 palette source review

## Confirmed generic correction

The internal palette table is documented/stored as ARGB DWORD entries.
d3d8_to_dxgi_format selects B8G8R8A8_UNORM for P8 expansion, but the converter
wrote bits 16..23 into the B byte and bits 0..7 into R. It now writes the low
blue byte first and red at byte two, preserving green/alpha. CPU expected-byte
and boundary checks fail before the fix. A real D3D11 WARP sample/readback
extension also reports 112 P8 failures before the fix and zero afterward.

The existing GPU sweep now includes P8 for 2D/cube/volume textures, all four
stages, four alpha values, fixed-function/combiner paths and the 2D programmable
VS preservation path. The 448 texture plus four RHW draws pass, alongside
format, gamma, bound-state and vertex-program programs. Both game builds pass.
Logs: conformance_tmp/ownership-p8-*.log. This corrects this D3D8 object-layer
path; it is not proof of the separate NV2A renderer, POSIX GL or live MM3 colors.

## Unresolved default-palette provenance

The Windows device generates 256 opaque grayscale ARGB entries on first
palette use; d3d8_SetPalette(NULL) regenerates the ramp. A standalone probe
linked to the actual owned D3D8 library, without creating a device/window,
confirms every default entry, explicit palette retention, and NULL restoration:

    default=opaque grayscale (256 entries); explicit data retained; NULL restores grayscale

Probe: conformance_tmp/ownership-palette-probe/probe.c and corresponding
config/build/output logs. Source: d3d8_device.c palette initialization/accessors
and dev_SetPalette; consumer: d3d8_resources.c P8 expansion.

No independently verified original SDK default has yet been established in
this review. The existence of generated colors alone does not prove they are
application policy. Establish their provenance against original initialization
or a directly applicable SDK/hardware contract before changing behavior.
If they are only a host diagnostic fallback, move selection into the consuming
port; the existing explicit d3d8_SetPalette interface already accepts the data,
so a new callback/configuration abstraction is not required merely to supply
that palette. If they implement the actual shared API default, retain them with
that evidence. Default behavior remains unchanged while this question is open.

The grayscale default cannot expose a red/blue swap because its channels are
equal. Thus the new regression uses asymmetric caller-owned colors. This does
not validate complete palette lifetime/stage association, missing-palette
semantics, format constants, device reset or the rest of the resource/device
implementation. The broader ownership audit remains open.

## Independent reference check

Microsoft's public Direct3D 9 texture-palette documentation specifies ARGB
entries, a current palette shared across stages, and all-white default entries:
https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-palettes
This is a PC D3D9 contract, not proof of the original Xbox D3D8 initialization.
Do not replace the ramp with white by transferring that contract silently.

Cxbx-Reloaded source was inspected at commit
585c49a50af1255ab155099e06f24505f9c5a800. Its Direct3D9.cpp declares initially
null cached Xbox palette pointers (line 196). CxbxImpl_SetPalette records data
and size from an Xbox palette resource (9230-9242), while hooked SetPalette
also calls original guest code for resource references (9293-9309). The missing-
palette upload path at 6368-6377 zero-fills the destination and explicitly
identifies that behavior as an uncertain crash-avoidance fallback. That is
not independent hardware validation of a zero, white or grayscale default.

Pinned source:
https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/585c49a50af1255ab155099e06f24505f9c5a800/src/core/hle/D3D8/Direct3D9/Direct3D9.cpp
SHA-256 of downloaded bytes:
3fe69c71903f2280cb586c7bdbff00fb99bba80e3951473f5b67349e87e0c3d4.
Pinned XbConvert.cpp was also inspected for palette expansion (894-899); its
lookup/copy does not establish missing-palette semantics. SHA-256:
2d84f56a5f9ed4206250970da674daa657a52eb1ada69f37b9a613a09a93bf80.
Downloaded reference copies remain under conformance_tmp/palette-reference;
no external implementation code was incorporated into the fork.

Result: original initialization/default semantics and its ownership decision
remain unproven. Next useful evidence is original Xbox initialization/binding
behavior or a directly applicable SDK contract, not another emulator's admitted
fallback. Other source review can continue independently; this does not block
the entire ownership goal or authorize changing the default by analogy.
