# Texture cache and upload regression checks

Run from the game checkout with its portable Windows toolchain:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
. ./scripts/enter-env.ps1
cmake -S tools/xboxrecomp/tests/nv2a_textures -B conformance_tmp/texture-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build conformance_tmp/texture-tests
ctest --test-dir conformance_tmp/texture-tests --output-on-failure
```

The four GPU cases exercise failed-resource retry, complete mip publication,
retention of an existing upload during allocation failure, and independent
palette-length cache identity. The CPU case compares complete ARGB/XRGB and
DXT1/3/5 rectangles with independent per-texel addressing and decoding. It covers
unaligned source addresses, odd compressed sizes, short mip dimensions, and
destination boundary guards.

The `native_bc` case samples BC1/2/3 resources on the GPU against an independently
decoded BGRA reference. Its 120 combinations cover full and short mip chains,
all cube faces, non-square and non-power-of-two dimensions, four source
alignments, point and shifted linear filtering, actual resource/SRV formats and
byte accounting, failed-view retry, and streamed updates across kick epochs.
Base dimensions that are not multiples of four, unsupported formats, and chains
that need generated mips retain the existing BGRA path.

Native BC decoding preserves the guest blocks but is not bit-identical to the
integer CPU decoder. The test derives each texel/channel's tolerance from the
endpoint span under [Direct3D's BC precision rules, section 19.5.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#19.5.2),
plus the exact difference from the CPU decoder's expansion/truncation. Filtered
bounds use the contributing texels' weights. Transparent/opaque extrema and
BC2's explicit alpha are checked exactly. The RTX 2000 Ada's largest differences
in the randomized tests were 6/255 for RGB and 7/255 for intermediate alpha.

The optional CPU microbenchmark compares the original row traversal with the
selected decoder, using medians of seven runs. It has no timing pass/fail limit:

```powershell
./conformance_tmp/texture-tests/nv2a_textures.exe benchmark_decode
```

On the Ryzen 9 PRO 7940HS, a 1024x1024 upload took 0.248 ms for ARGB32 and
0.209 ms for XRGB32, compared with 1.595 ms and 1.469 ms for the original
traversal. DXT1/3/5 improved by 1.27-1.37 times. These measurements cover CPU
decoding, not GPU upload or whole-frame time, and vary with load and clock speed.

The optional native-upload benchmark times CPU hashing, decoding and upload API
calls for already-created 1024x1024 textures with all 11 mip levels:

```powershell
./conformance_tmp/texture-tests/nv2a_textures.exe benchmark_native_bc
```

It alternates measurement order, reports seven-sample medians of eight updates,
and drains GPU completion between samples outside the measured interval. On
this PC, BGRA/native CPU costs were 3.171/0.103 ms for DXT1, 3.822/0.170 ms for
DXT3, and 3.078/0.183 ms for DXT5. Texture/upload data fell from 5,592,404 bytes
to 699,064 bytes for BC1 and 1,398,128 bytes for BC2/3. This measures eligible
upload bursts; it does not measure GPU throughput or predict gameplay FPS, and
does not apply to short mip chains that retain generated BGRA mips.

For an actual renderer snapshot comparison, configure another build with
`-DNV2A_TEXTURE_RENDERER=C:/absolute/path/to/nv2a_d3d11.c`. The source's sibling
headers can be resolved through the renderer include directory supplied by this
test project.
