# Generic D3D8 vertex decoder correction frontier

This records the original generic defect and the subsequent wire/paired-unit
correction below. Full translator and caller fidelity remains unproven.

## Independent reproduction

The actual object-layer parser accepts four DWORDs per instruction. A literal
MOV oPos.xyzw, c[191].xyzw instruction, final, is encoded as:

    00000000 0037E01B 0C000000 0000F801

The probe tools/xboxrecomp/docs/reference/ownership_vsh_encoding_probe.c uses the production
parser through the existing test host stubs, without constructing instructions
with its own field macros. It compiles successfully and exits 1 with:

    wire MOV c191: mac=0 constant=0 source_type=0 source_index=0 output=255 final=1; FAIL

Expected: MAC MOV, constant 191, constant source 191 with identity swizzle,
oPos destination, final. No device/window is created by this probe. Evidence:
conformance_tmp/ownership-vsh-encoding-{config,build}.log and output .log.
The existing test_vsh make_const_mov uses the production field macros, so its
passing constants/layout tests cannot establish independent wire decoding.

From a Windows x64 MSVC developer shell, with build/ present:

```bat
cl /nologo /Itools\xboxrecomp\src tools\reference\ownership_vsh_encoding_probe.c /Fo:build\ownership_vsh_probe.obj /Fe:build\ownership_vsh_probe.exe /link d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib
build\ownership_vsh_probe.exe
```

The diagnostic originally failed as recorded above; it now passes after the
correction below. Independent wire cases have been promoted into the generic
suite. This single probe is not complete translator acceptance.

## Pinned reference

xemu commit 478b4f496102379c7eaa7f3ec10e714a703c4300, field_mapping in:
https://github.com/xemu-project/xemu/blob/478b4f496102379c7eaa7f3ec10e714a703c4300/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c#L47-L93
Downloaded SHA-256:
dbb4b71d99d91b15a0b2fd61dd297929d4d3fb0a5f964e68dc6506debb2df01e.
Associated vsh_regs.h SHA-256:
7b94b5a65d1d095b75316a04e401faeaa44fa1fa5918bb5d3653376383694a69.
Reference copies remain in conformance_tmp/vsh-reference. No external
implementation code has been incorporated into the fork in this checkpoint.

The reference places MAC, ILU, constant/input fields in DWORD 1; the object
translator reads those from DWORD 0. Real source mux values distinguish
unknown/temp/input/constant as 0/1/2/3, unlike the current parser's mapping.
A/B/C fields occupy different words/positions; simply shifting every field
by 32 does not repair the decoder. The existing kernel interpreter's encoding
comment agrees with this reference, but that does not certify its full behavior.

## Original correction checklist

- Trace parser callers and guest shader-upload adaptation before changing
  interpretation. Use independent literal/assembler fixtures, not a constructor
  driven by the implementation's own field macros.
- Correct opcode widths/locations, operand muxes, cross-word C register fields,
  swizzles and relative addressing. Validate ignored DWORD 0, final termination,
  constant/input usage and nontrivial A/B/C combinations.
- Model separate MAC/ILU temp masks and the shared output mask, type/address
  and unit selection. The current destination representation shares a write
  mask between temporary/output writes and needs review with the emitter.
- Verify paired-unit read/write ordering and paired ILU temporary destination
  against the reference. Review constant-output/state-program support and
  unsupported-program error propagation instead of silently claiming support.
- Preserve useful existing constant-boundary and layout-cache tests, while
  replacing their circular encoding construction. Verify real generated HLSL
  compilation and representative GPU results before broad translation claims.

Decoder and test source retain needs_generic_fix for the remaining wider
translation/caller work. This work belongs in the generic fork;
no MM3 shader-name/address branch or title-specific replacement is warranted.

## Implemented wire and paired-unit checkpoint

The decoder now uses the pinned wire locations/muxes, cross-word C temporary
index and relative bit. It tracks only actually read ports. The IR separates
temporary/output masks and retains constant-output addresses; unsupported
constant-file writes/encodings are rejected before a shader handle is allocated.
Missing input attributes are not fabricated for unused ports. No-input programs
generate a no-input HLSL signature and compile.

The emitter snapshots used operands before either unit writes. It computes both
results before committing outputs/temporaries, suppresses paired MAC R1 writes,
forces paired ILU temporary writes to R1 and commits paired MAC temporaries
after ILU as in the inspected reference. Original caller/ABI and draw-failure
propagation are still separate review scopes.

Independent tests cover ignored DWORD 0, literal MOV c191, source muxes, C's
cross-word index, actual input usage, relative addressing, asymmetric temp/output
masks, paired R1 rules, unsupported state-write rejection and unchanged failed
output handles. All 20 MAC/ILU opcode emissions compile; this is syntax proof,
not numerical conformance of every opcode. WARP readback of a three-instruction
program checks an ILU reading the pre-MAC R2.x value (0.25 -> red byte 64),
while the paired MAC writes 0.75. The historical standalone wire probe passes.

All five Windows D3D8 programs pass and both game builds pass. Logs:
conformance_tmp/ownership-vsh-{fix,suite}-*.log, ownership-vsh-ownership-*-build.log
and ownership-vsh-encoding-fixed.log. Initial GPU-test failure was diagnosed
with the D3D11 debug layer as mismatched VS/PS signature registers; the test's
pixel signature now matches the generated VS. Final verification uses a normal
WARP device and succeeds. No production change was made to hide that failure.

Remaining: complete ALU numeric behavior (including EXP/LOG/RCC and corner
cases), register/constant state semantics, guest upload/ABI adaptation, stream
declarations, cache identity, complete unsupported-draw handling, other backend
parity and arbitrary-program/gameplay validation. This is concrete generic
progress, not a claim that the translator or wider ownership goal is complete.
