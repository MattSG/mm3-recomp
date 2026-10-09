# Xbox D3D8 / NV2A → D3D11 translation audit

Date: 2026-10-06. Source audit of the current working tree, not a gameplay or hardware-conformance result.

Root HEAD: `67a3157a35c59d4d62fbc166fdfa538a21385df4`.
`tools/xboxrecomp` HEAD: `340d2b5b958412e3e914de2e6dd67f97f3928c0d`.
Both checkouts already contain local changes; the findings describe the files currently on disk, not just those commits. No renderer, game data, configuration, generated code, or running process was changed for this audit.

## What this means for “real recomp”

The game CPU code is statically recompiled. D3D11 renders the translated graphics work. The graphics runtime still implements Xbox/NV2A behavior; translating commands, shader instructions, texture layouts, and hardware state is a compatibility implementation of GPU semantics. That does not turn the CPU recomp into xemu.

There is no requirement to remove every NV2A compatibility operation to qualify as a recomp. The work below is about rendering correctness and compatibility. Replacing it with intercepted high-level game rendering calls would be another architecture, not a prerequisite for static recompilation.

## Read this distinction first

There are **three different rendering implementations** in this tree. They must not be combined into one misleading completeness percentage.

| Path | Entry and destination | Relevance |
| --- | --- | --- |
| **A: MM3 GPU-command path** | Guest DMA PUT → `xbox_memory_layout.c` → `nv2a_pb_scan.c` → `kernel/nv2a_pb_exec.c` → `video/nv2a_d3d11.c` | The important path for original game-generated GPU commands when `RECOMP_PB_EXEC` is enabled. D3D11 is selected unless the software-raster override is used or initialization fails. |
| **B: D3D8 wrapper** | `xbox_Direct3DCreate8` / the custom `IDirect3DDevice8` vtable → `src/d3d/d3d8_*.c` → D3D11 | A separate native compatibility API; `src/host_graphics.c` creates a host device. A missing wrapper feature is not automatically a missing feature in A. |
| **C: legacy PGRAPH/replay renderer** | `pgraph_method` → `nv2a_pgraph_d3d11.c`; replay/test executables also use it | A limited prototype. It is not the destination called by A's executor. MMIO/PVIDEO support is still relevant, but that does not make C the MM3 draw backend. |

Evidence: [DMA submission](tools/xboxrecomp/src/kernel/xbox_memory_layout.c#L1001), [scan execution](tools/xboxrecomp/src/kernel/nv2a_pb_scan.c#L225), [D3D selection/state transfer](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L2505), [actual draw](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L2672), [GPU vertex processing](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2890), [host wrapper initialization](src/host_graphics.c), [legacy dispatch](tools/xboxrecomp/src/nv2a/nv2a_core.c#L566).

Some introductory comments in `nv2a_pb_scan.c` and `nv2a_pb_exec.c` still describe a read-only scanner or a future upgrade to C. Current call sites take precedence over those comments.

### Status and impact labels

- **Missing**: no implementation or the operation is ignored/rejected.
- **Defect**: implementation exists but the source shows a concrete mismatch.
- **Partial**: some variants work; the stated variants do not.
- **Approximation**: a substitute exists but does not preserve exact Xbox behavior.
- **Verification gap**: implementation exists, but source inspection does not prove equivalence.

“A” or “B” means the affected implementation, not proof MM3 exercises the feature. No new runtime trace was collected. **A source-visible gap is confirmed; its contribution to a particular MM3 visual bug is not confirmed by this report.**

## 1. Highest-value findings in MM3's GPU-command path

| ID | Status | Translation still needed | Evidence / consequence |
| --- | --- | --- | --- |
| A01 | Missing | NV2A fixed-function transform, lighting, skinning, texture generation and matrix state | `raster_batch_impl` sends programmable mode 2 or recognized screen-space batches to D3D11; other batches increment `batches_untransformed` and are skipped. The separate wrapper's fixed-function shader does not fill this hole. [Executor](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L2752). |
| A02 | Missing | True volume/3D textures and projective 3D texture-shader sampling | Texture descriptors/layout track width, height, faces and levels, but not volume depth. Host textures are `Texture2D`/cube. HLSL mode 2 shares the 2D sampling branch. GPU format dimensionality/base-size-P is not translated into a volume resource. [Texture methods](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3350), [HLSL](tools/xboxrecomp/src/video/nv2a_d3d11.c#L539), [layout](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1886). |
| A03 | Partial | Vertex-program constant/context writes and vertex-state-program behavior | GPU HLSL explicitly drops writes to the constant file. CPU interpreter can write `s_const` when context writes are enabled, so the implementations differ; incidental CPU transforms used for widescreen placement are not a correct replacement for GPU execution semantics. [GPU write handling](tools/xboxrecomp/src/video/nv2a_d3d11.c#L465), [CPU interpreter](tools/xboxrecomp/src/kernel/nv2a_vsh_interp.c#L414). |
| A04 | Missing | Depth/stencil guest-memory coherence and depth-texture feedback | `depth_get` always creates a fresh D24S8 host surface and clears it; it does not import guest zeta bytes. It has no sampleable depth SRV, no depth write-back, and no depth alias path through `surface_find`. Texture decoding of depth bytes is not the same as sampling the host-rendered depth buffer. [Depth allocation](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1657), [write-back](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3217). |
| A05 | Partial | Color-surface coherence for swizzled targets and arbitrary CPU reads | `surface_sync` excludes swizzled targets; write-back excludes them too. GPU output reaches linear 16/32-bit guest surfaces at flip, not at every CPU read/fence. Uploads are shadow comparisons at submission epochs, not page-level coherence. CPU read-before-flip, CPU writes to swizzled targets, reinterpretation/overlapping allocations remain incomplete. [Sync](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1475), [write-back](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3217). |
| A06 | Missing | Object-class/subchannel dispatch and full DMA context objects | Executor assumes the 3D class is on subchannel 0; other subchannels are counted as unhandled. `dma_resolve` uses allocator/address heuristics rather than context-object base/limit semantics. NV062/NV089/NV09F/other blit/copy classes are not translated by this dispatcher. [Address resolver](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L64), [subchannel gate](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3464). |
| A07 | Missing | Backend semaphore/fence method execution | `SET_SEMAPHORE_OFFSET` and `BACK_END_WRITE_SEMAPHORE_RELEASE` have no executor case. Existing MMIO completion/frame-counter/fence mirrors do not establish execution of these methods or correct GPU-completion ordering. [Method switch](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3472), [register definitions](tools/xboxrecomp/src/nv2a/nv2a_regs.h). |
| A08 | **Fixed** (xboxrecomp 2f09ffe) | Small point/line batches | Common raster entry rejects `idx_count < 3` before selecting a topology. Inline/immediate paths also have minimum-three guards. `build_list` and D3D11 can render points/lines, but a single point or two-vertex line cannot reach them through these guards. [Batch guard](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L2757), [inline/immediate](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3158). |
| A09 | Missing | Point size/attenuation/sprites, two-sided vertex colors and associated raster state | Vertex-program output registers for point size and back colors are not delivered as these features to the pixel/raster pipeline. Point raster methods are not handled. Host point-list support alone does not implement Xbox sized points or sprites. [Output mapping](tools/xboxrecomp/src/video/nv2a_d3d11.c#L474), [method switch](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3410). |
| A10 | **Fixed** (xboxrecomp 2f09ffe) | Sampler cache identity including V/W addressing | `sampler` key retains only `address & 0xFFF`, while its descriptor reads W from bits 16..19 and V from bits 8..11. W changes cannot change this key; other address control fields are also omitted. Full descriptor equality/keying is needed for every translated field. [Sampler](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2336). |
| A11 | **Fixed** (xboxrecomp 2f09ffe) | Correct state behavior after cache saturation | `state_slot` returns an already occupied slot when all 512 slots are full. Callers create a state only when the slot is NULL, so the requested new state can reuse an unrelated old state. This affects blend, depth/stencil, raster and sampler caches. The “leaks one object” comment understates the current behavior. [Cache](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2123). |
| A12 | **Partly fixed** (2f09ffe: F005/F006 mapping; signed range and mixed constant factors remain) | Signed blend equations and mixed constant-color/constant-alpha factors | `0xF005` is defined as reverse-subtract-signed but mapped to subtract; `0xF006` is add-signed but mapped to reverse-subtract. Also, when either factor uses constant alpha, one replicated alpha blend vector is used for both factors, changing a simultaneously requested constant-color factor. Signed behavior needs an explicit emulation strategy, not these aliases. [Definitions](tools/xboxrecomp/src/nv2a/nv2a_regs.h#L1000), [blend operation](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2160), [blend vector](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2809). |
| A13 | Approximation | Native guest render-target and depth precision | All color targets are BGRA8 regardless of guest color format. RGB565 is quantized on clear/write-back, but normal draw/blend storage stays 8-bit. Depth allocation is always D24S8, including guest D16 and floating-depth choices. Exact format precision during multipass blending/depth tests is not preserved. [Surface allocation](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1624), [depth](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1684). |
| A14 | Partial | Hardware report types, timing and exact visibility results | GET_REPORT ignores report-type bits and emits a synthetic timestamp; it treats reports as z-pass results with physical-zero DMA assumptions. Host occlusion samples are scaled back to estimated guest pixels. Z-pass exists, but full report semantics and exact counts do not. [GET_REPORT](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3535), [occlusion conversion](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3340). |
| A15 | Partial | Scanout/flip/vblank ordering and display modes | FLIP_STALL immediately sets read=write and advances the title counter before host presentation. Scanout fallback assumes 480 lines and derives a capped width/format from pitch. Host pacing and swapchains are implemented, but exact PAL/interlace/refresh/sample-grid/scanout scheduling is not. [Flip](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3632), [scan fallback](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3750). |

### Other source-visible limitations in A

1. **Texture cache invalidation within a frame:** a cache entry checked once in `s_frame` is returned without rehashing on a later use in that frame. CPU writes or palette edits between draws can be missed until a later frame. Palette length is used for hashing but is not an independent cache-identity field; palette indexing does not explicitly enforce that length. See [texture_get_impl](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2020).
2. **Texture format/dimensionality limits:** `tex_format` rejects unlisted formats. Its decoded upload always becomes BGRA8, losing higher-precision/signed/depth distinctions. Floating depth formats are converted like fixed/integer channels. Unknown formats yield no view rather than a translated texture. The NV097 table is a separate namespace from the wrapper's extended D3DFORMAT enum; do not demand every wrapper-only format from NV097 blindly. See [format/decode](tools/xboxrecomp/src/video/nv2a_d3d11.c#L1764).
3. **Depth texture shader modes:** mode 10 computes dot-related intermediate values, but does not supply the requested texture-generated depth through an equivalent depth output. `ps_wdepth` is W-buffer depth, not general DOT_ZW/shadow-texture support. Shadow compare controls are unhandled. Modes outside the enumerated branches fall back to default texture results. Dependent/dot/reflection modes are implemented in part; they still need conformance tests. See [texture-stage HLSL](tools/xboxrecomp/src/video/nv2a_d3d11.c#L625).
4. **Fog generation:** fog modes/parameters and programmable fog output exist; `SET_FOG_GEN_MODE`, fog plane and full fixed-function fog generation state do not. Do not label all fog “missing.”
5. **Clipping/raster:** only surface-derived scissor is applied. Window-clip regions/type, clip-min/max, ZMIN_MAX policy, polygon point/line modes, line/polygon smoothing, point/line polygon offset, dither, and provoking-vertex override are unhandled. Rasterizer always uses solid fill and disables depth clipping. Flat shading always rotates indices to use the last guest vertex; the explicit provoking-vertex method is not translated. See [raster state](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2251), [index rotation](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2835).
6. **Guest AA versus host AA:** surface-format AA grid affects dimensions, but actual samples come from the global host `RECOMP_MSAA` choice. Anti-alias control/sample masks and Xbox quincunx/Gaussian/supersample resolve semantics are not implemented faithfully. Swizzled render targets stay single-sampled.
7. **Texture controls:** CONTROL2-range words can be silently consumed by the texture-range dispatcher without an implementation. Texture color-key state, border-source-in-texture behavior, shadow depth comparison and exact special filters are absent/partial. Address modes outside the explicitly mapped set fall back to clamp. Anisotropy is a global enhancement rather than a full per-guest control translation.
8. **Capacity handling:** push batches silently stop adding indices/inline data at fixed limits. D3D11 ring-buffer requests that exceed capacity return without drawing. No split/retry translation covers large valid guest batches. Program execution has the 136-slot limit, but malformed/missing termination and instruction corner cases are not diagnosed comprehensively.
9. **Aliasing:** color surfaces are found by matching base physical address and recent binding; this is not a general overlap/format reinterpretation model. Cache matching can update `color_fmt` on an existing same-bpp target rather than reconstruct format-specific semantics. Eviction/recreation and resize discard host depth state; swizzled GPU content has no guest write-back to recover from.
10. **Error propagation:** many shader/view/state creations ignore their HRESULT. A draw counter can advance after a draw function silently returns. Counts are useful diagnostics, not pixel-correctness or successful-host-draw proof.

## 2. NV097 method coverage: handled and absent families

This inventory compares the executor's switch/range handlers with the local `nv2a_regs.h` method definitions. A missing method may be irrelevant to MM3 or belong to another object class; it remains a compatibility gap, not automatically a required next task.

| Family | Present in A | Missing or partial in A |
| --- | --- | --- |
| Object/DMA setup | Heuristic physical resolution | SET_OBJECT binding; context DMA notification/A/B/state/color/zeta/vertex/semaphore/report objects and their limits; nonzero-subchannel classes |
| Surface | Clip H/V, format, pitch, color/zeta offset | Native color/depth precision, complete aliasing/coherence |
| Combiners | All 8 RGB/alpha ICW/OCW/factors, final controls and independent final constants, combiner control | Hardware precision/rounding proof; unsupported texture modes are separate from combiner math |
| Alpha/blend | Enable/function/reference, source/destination factors, blend color/equation, channel mask | Signed blend equations; mixed color/alpha constant factors |
| Depth/stencil | Depth enable/function/mask; stencil enable/masks/function/ref/ops; W-depth; clear values | ZMIN_MAX/clip-min/max policies, float-depth storage, guest depth memory, shadow comparison |
| Fog | Enable/color/mode and first two fog parameters; programmable fog output | FOG_GEN_MODE, fog plane, complete fixed-function path |
| Raster | Cull enable/face/front-face, shade mode, fill polygon offset | Dither; point/line/poly smoothing; point parameters/size/sprites; front/back polygon modes; point/line offset; normalization; provoking-vertex selection |
| Window clip | Surface-derived scissor | WINDOW_CLIP_TYPE, eight horizontal/vertical window-clip regions |
| Fixed T&L | Programmable vertex programs and constants | LIGHT_CONTROL, COLOR_MATERIAL, LIGHTING_ENABLE, SKIN_MODE, MATERIAL_EMISSION/ALPHA, SPECULAR_ENABLE, LIGHT_ENABLE_MASK, front/back light data, scene ambient, specular params |
| Matrices/texgen | Viewport scale/offset placed in c58/c59 for vertex-program epilogues | Projection/model-view/inverse/composite/texture matrices, texture-matrix enable, TEXGEN S/T/R/Q and planes/view-model, eye-position/vector fixed-function use |
| Submission | BEGIN_END, DRAW_ARRAYS, 16/32-bit elements, INLINE_ARRAY, 16 attribute arrays/formats, immediate attribute families, vertex3/4f | Small line/point guards, capacity splitting; unsupported vertex format types |
| Primitive conversion | Triangle list/strip/fan, quads/quad strip/polygon, lines/strip/loop and points | Exact flat colors for split quads/polygons and explicit provoking-vertex behavior need verification |
| Texture state | Offset/format/address/control0/1/filter/image rect/palette/border/bump matrix/scale/offset on 4 stages | Depth/P dimension, CONTROL2, color-key colors, full border-source/filter/AA/shadow controls |
| Texture shader controls | Stage program, dot RGB mapping, other-stage input and clip-plane control | 3D sampling, depth-producing/shadow cases and unrecognized modes |
| Clear | Color channel masks, Z/stencil, clear rectangle | Native float-depth encoding/precision; degenerate all-zero clear rectangle needs explicit-state verification |
| Reports | Z-pass clear/enable/read | Other report types, real timestamp, report DMA object, exact native sample counts |
| Flip | Read/write/modulo, increment, stall/present | Real completion/scanout schedule, full display mode handling |
| Synchronization | Submission kick and existing MMIO mirrors | SET_SEMAPHORE_OFFSET / BACK_END_WRITE_SEMAPHORE_RELEASE; full hardware object/interrupt ordering |

Important examples defined locally but absent from the executor: `SET_LIGHT_CONTROL` 0x0294, `SET_COLOR_MATERIAL` 0x0298, `SET_FOG_GEN_MODE` 0x02A0, window clip 0x02B4/0x02C0/0x02E0, raster/lighting 0x0310–0x0328, point/line offset 0x0330/0x0334, polygon/clip 0x038C–0x0398, material/light 0x03A4–0x03BC, texgen starting 0x03C0, texture-matrix enable 0x0420, point size 0x043C, matrices 0x0440–0x07xx, texgen planes 0x0840 onward, provoking vertex 0x09FC, point params 0x0A30, color-key 0x0AE0, semaphore 0x1D6C/0x1D70, ZMIN_MAX 0x1D78, anti-aliasing 0x1D7C, shadow 0x1E68/0x1E6C. Ranges describe families, not an assertion that every word in each range is a distinct method.

Sources: [method definitions](tools/xboxrecomp/src/nv2a/nv2a_regs.h#L899), [entire dispatch](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3410), [texture dispatch](tools/xboxrecomp/src/kernel/nv2a_pb_exec.c#L3350).

## 3. Generic D3D8 wrapper: device/API gaps

| ID | Status | Gap | Source |
| --- | --- | --- | --- |
| B01 | Missing | `GetDirect3D` returns E_NOTIMPL. `QueryInterface` rejects all interfaces, including basic identity queries. | [Device](tools/xboxrecomp/src/d3d/d3d8_device.c#L365), [factory](tools/xboxrecomp/src/d3d/d3d8_device.c#L1498); same pattern in resources |
| B02 | Missing | `GetDeviceCaps`, `GetDisplayMode`, `GetCreationParameters` return success without populating output; Reset returns success without recreating/resizing. | [399–430](tools/xboxrecomp/src/d3d/d3d8_device.c#L399) |
| B03 | Partial | CreateDevice ignores adapter/type/behavior. Swapchain forces RGBA8, single-sample discard behavior; default depth forces D24S8. Requested backbuffer/depth formats, AA, swap effect, auto-depth disable and presentation interval are not fully translated. | [Creation](tools/xboxrecomp/src/d3d/d3d8_device.c#L229), [depth](tools/xboxrecomp/src/d3d/d3d8_device.c#L303) |
| B04 | Partial | Present ignores source/destination rectangles, override window and dirty region; Swap ignores flags; presentation uses fixed sync interval. Gamma lookup exists, but SetGammaRamp flags are ignored. | [Present](tools/xboxrecomp/src/d3d/d3d8_device.c#L439), [Swap](tools/xboxrecomp/src/d3d/d3d8_device.c#L1398), [gamma](tools/xboxrecomp/src/d3d/d3d8_device.c#L1358) |
| B05 | Partial | GetBackBuffer ignores index/type and wraps buffer 0 with fixed descriptor assumptions. Default GetRenderTarget/GetDepthStencilSurface return NULL rather than default surfaces. | [Backbuffer](tools/xboxrecomp/src/d3d/d3d8_device.c#L482), [target getters](tools/xboxrecomp/src/d3d/d3d8_device.c#L1214) |
| B06 | Missing | BeginPush/EndPush return E_NOTIMPL. This is independent of A, whose guest RAM pushbuffer walker exists. | [Push API](tools/xboxrecomp/src/d3d/d3d8_device.c#L1385) |
| B07 | Missing | SetPixelShaderConstant is a no-op returning success. Shader create/delete/query APIs are not exposed in the public device vtable; the static CreateVertexShader function/helper does not fix public reachability and ignores declaration/usage. | [Shader methods](tools/xboxrecomp/src/d3d/d3d8_device.c#L1300), [PS constants](tools/xboxrecomp/src/d3d/d3d8_device.c#L1352), [vtable](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L1020) |
| B08 | Missing | Streams beyond 0 return success without binding. NULL stream/index/texture bindings do not reliably clear previous D3D11 bindings; a texture without an SRV can leave stale host state. | [Texture/streams](tools/xboxrecomp/src/d3d/d3d8_device.c#L615) |
| B09 | Defect | FAN/QUAD counts are expanded, but DrawPrimitive, DrawIndexedPrimitive and DrawIndexedPrimitiveUP do not expand their vertices/indices. Only non-indexed UP performs conversion. Allocation failure there can fall through with original data plus expanded draw count. | [Topology](tools/xboxrecomp/src/d3d/d3d8_device.c#L706), [draw calls](tools/xboxrecomp/src/d3d/d3d8_device.c#L841) |
| B10 | Partial | Indexed UP ignores MinVertexIndex/range handling. Validation of submitted ranges, buffer locks and invalid state values is incomplete. | [Indexed UP](tools/xboxrecomp/src/d3d/d3d8_device.c#L933) |
| B11 | Missing | Clear ignores Count/pRects and clears whole targets. | [Clear](tools/xboxrecomp/src/d3d/d3d8_device.c#L521) |
| B12 | Partial | SetRenderTarget falls back to default depth when NULL depth is requested, preventing depth detachment. It releases old resources before full validation, can succeed with invalid depth, and does not fully update viewport/target-dependent fixed-function coordinate conversion. | [Target binding](tools/xboxrecomp/src/d3d/d3d8_device.c#L1175) |
| B13 | Defect | Bound texture/VB/IB pointers are not retained consistently; resource GetDevice does not AddRef. Parent-resource lifetime for palette surfaces is incomplete. Getter/binding COM ownership needs a coherent implementation. | [Bindings](tools/xboxrecomp/src/d3d/d3d8_device.c#L615), [resource methods](tools/xboxrecomp/src/d3d/d3d8_resources.c#L743) |
| B14 | Partial | Only eight stored light slots; larger indices silently disappear. Resource pools/usage/priority/preload semantics largely ignored or simplified. | [Lights](tools/xboxrecomp/src/d3d/d3d8_device.c#L1260), [creation](tools/xboxrecomp/src/d3d/d3d8_device.c#L1007), [resources](tools/xboxrecomp/src/d3d/d3d8_resources.c#L750) |

### API families absent from the wrapper interface

The header exposes a deliberately reduced custom vtable, not a complete XDK compatibility surface. The following families have no equivalent public operation here: state blocks/capture/apply; copy/update textures and surfaces; clip-plane setup/query; multiply-transform; shader declaration/function retrieval and normal public creation/deletion; visibility/fence/query operations; resource registration/move-memory/busy/block-until-idle/fixups and full pushbuffer lifecycle; resource private data; texture LOD and dirty-region APIs; factory adapter enumeration and format/capability checks.

These are **interface coverage gaps**, not proof that recompiling MM3 needs every one. The header's claim about matching Xbox binary vtable layout is not independently verified by this audit; appended host extensions and LTCG/direct guest calls mean binary ABI correctness requires explicit validation. See [public interfaces](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L755), [device interface](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L1010).

## 4. Wrapper render-state and texture-stage coverage

### Render states declared in the local header

All these values can be stored by SetRenderState; storage/round-trip is not rendering support.

| State group | Translation status in B |
| --- | --- |
| ZENABLE, ZWRITEENABLE, ZFUNC | Ordinary Z supported; W-buffer enable is treated as boolean Z rather than W-depth |
| STENCILENABLE, STENCILFAIL/ZFAIL/PASS, STENCILFUNC/REF/MASK/WRITEMASK | Implemented in D3D11 depth/stencil state; do not list these as absent |
| ALPHABLENDENABLE, SRCBLEND, DESTBLEND, BLENDOP, COLORWRITEENABLE | Basic mapped blend factors/ops/write masks implemented; unmapped factors default to ONE |
| ALPHATESTENABLE, ALPHAFUNC, ALPHAREF | Implemented in pixel shader; exact integer/rounding equivalence remains unproven |
| FILLMODE, CULLMODE | Solid/wireframe/cull implemented; point fill substitutes wireframe |
| SHADEMODE | Stored, not used to select flat/nointerpolation shading |
| DITHERENABLE, EDGEANTIALIAS | Stored, no equivalent raster implementation |
| FOGENABLE/COLOR/TABLEMODE/START/END/DENSITY/VERTEXMODE | Shader implementation exists; pretransformed vertex fog and vertex fog source semantics partial |
| RANGEFOGENABLE | Used for table fog; vertex fog still computes radial distance regardless |
| SPECULARENABLE, LIGHTING, AMBIENT | Fixed-function shader implementation exists |
| COLORVERTEX, LOCALVIEWER, NORMALIZENORMALS | Stored but requested behavior not respected; shader normalizes and uses its fixed lighting conventions |
| DIFFUSE/SPECULAR/AMBIENT/EMISSIVEMATERIALSOURCE | Stored, not used to choose material versus vertex color |
| VERTEXBLEND | Weight fields affect layout offsets, but no blend/skinning transform |
| WRAP0/1/2/3 | Stored, no D3D8 coordinate-wrap interpolation translation |
| POINTSIZE, POINTSIZE_MIN, POINTSPRITEENABLE, POINTSCALEENABLE | Stored, no sized-point/sprite/attenuation implementation |
| MULTISAMPLEANTIALIAS, MULTISAMPLEMASK | Stored; raster flags fixed off and OM sample mask fixed all-ones |
| TEXTUREFACTOR | Implemented in fixed-function pixel shader |
| PSALPHAINPUTS0–7, PSRGBINPUTS0–7, PSALPHAOUTPUTS0–7, PSRGBOUTPUTS0–7, PSCONSTANT0_0–7, PSCONSTANT1_0–7, PSFINALCOMBINERINPUTSABCD/EFG, PSCOMBINERCOUNT, PSTEXTUREMODES | Read by wrapper combiner builder, but encoding/math defects below prevent claiming full support |
| PSDOTMAPPING, PSINPUTTEXTURE | Declared/stored, not translated by the wrapper combiner implementation |

Sources: [state enum](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L231), [native state builder](tools/xboxrecomp/src/d3d/d3d8_states.c), [fixed-function HLSL/upload](tools/xboxrecomp/src/d3d/d3d8_shaders.c).

### Texture-stage state

| State / operation | Status in B |
| --- | --- |
| COLOROP / ALPHAOP | SELECT/MODULATE/ADD/SUBTRACT and selected blend ops/DOT3 handled. PREMODULATE (16), MULTIPLYADD (25), LERP (26) have no corresponding operation and fall back to modulation. |
| COLORARG1/2, ALPHAARG1/2 | Basic sources/modifiers handled, but uploader treats zero as “unset.” `D3DTA_DIFFUSE = 0` is valid and gets replaced by fallback TEXTURE/CURRENT defaults. |
| COLORARG0, ALPHAARG0, RESULTARG | Stored but no three-argument operations/TEMP result routing. TEMP/CONSTANT argument sources absent from resolver. |
| BUMPENVMAT00/01/10/11 | Stored; fixed-function PS does not implement bump environment mapping. A has a separate implementation. |
| TEXCOORDINDEX | Coordinate selection and selected camera-space generation exist; pretransformed branch skips that generation path. |
| ADDRESSU / ADDRESSV | Mapped, including mirror-once; ADDRESSW is not exposed/applied and is always wrap. |
| BORDERCOLOR | Ignored; D3D11 border remains zero. |
| MAGFILTER / MINFILTER / MIPFILTER | Basic point/linear/aniso exist; mixed linear mip cases map incorrectly. MIPFILTER_NONE does not restrict sampling to mip zero. Quincunx/Gaussian filters lack correct equivalents. |
| MIPMAPLODBIAS / MAXMIPLEVEL | Ignored; MipLODBias/MinLOD default zero, MaxLOD unlimited. |
| MAXANISOTROPY | Applied without adequate host-range validation; failures can leave a previous sampler bound. |
| COLORKEYOP / COLORSIGN / ALPHAKILL | Stored, no fixed-function equivalent. Header assigns COLORARG0 and ALPHAKILL the same value 26, making these meanings ambiguous in one state array. |
| Texture transform flags/projected coordinates | No complete public flag translation; VS unconditionally applies texture matrices for nontransformed vertices and carries float3 texcoords, losing fourth/projective component. |

Sources: [stage enum](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L405), [op/argument resolver](tools/xboxrecomp/src/d3d/d3d8_shaders.c#L309), [shader constants](tools/xboxrecomp/src/d3d/d3d8_shaders.c#L1100), [sampler mapping](tools/xboxrecomp/src/d3d/d3d8_states.c#L253).

### Fixed-function vertex processing defects

- Pretransformed XYZRHW conversion uses backbuffer dimensions rather than a complete current-render-target/viewport mapping; its early return bypasses texture generation/matrices and fog calculation.
- Normals are always normalized and lighting uses the fixed eye/material convention. Material-source and vertex-color lighting controls are not honored.
- Blend weights are skipped in offsets but not used for skinning.
- Vertex fog source/range selection is incomplete; point-size and two-sided lighting are absent.
- Texcoord-size helper uses `field == 0 ? 2 : field`, but the local header's TEXCOORDSIZE3 macro encodes field 2 and TEXCOORDSIZE4 field 3. Even against its own macros, SIZE3 is read as 2 and SIZE4 as 3. This affects both fixed-function and wrapper programmable layouts. External XDK FVF encoding/ABI still needs a separate canonical check.

Sources: [FF VS](tools/xboxrecomp/src/d3d/d3d8_shaders.c#L120), [layout decoder](tools/xboxrecomp/src/d3d/d3d8_shaders.c#L689), [FVF definitions](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L655), [position-size helper](tools/xboxrecomp/src/d3d/d3d8_fvf.h).

## 5. Wrapper programmable vertex shader translation

The most serious B shader issue is not a missing opcode: **its raw microcode decoder disagrees with the real NV2A decoder already in this same tree.**

| ID | Gap / defect | Evidence |
| --- | --- | --- |
| V01 | Field positions are wrong for raw NV2A instructions: wrapper reads MAC/ILU/input/constant fields from word 0; active decoder reads MAC/ILU from word 1 and documents word 0 as unused. Source mux numbering and destination/relative fields also differ. | [Wrapper fields](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L119), [actual decode](tools/xboxrecomp/src/kernel/nv2a_vsh_interp.c#L36), [GPU decode](tools/xboxrecomp/src/video/nv2a_d3d11.c#L410) |
| V02 | Parallel MAC/ILU source snapshot and paired destination rules are not preserved. Wrapper emits MAC then ILU code without full pre-write snapshot and correct paired R1 ownership. | [Wrapper emission](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L963), [CPU pairing](tools/xboxrecomp/src/kernel/nv2a_vsh_interp.c#L393) |
| V03 | RCC clamp differs, including sign/range; EXP and LOG are scalar replicated operations instead of NV2A component tuples; LIT exponent bounds differ. | [Wrapper ILU](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L775), [CPU ILU reference](tools/xboxrecomp/src/kernel/nv2a_vsh_interp.c#L284) |
| V04 | Negative Xbox constant-register indices rejected; declaration and complete constant-address translation missing. Out-of-range decoded constants can collapse to zero. | [Setter](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L1402), [parser](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L265) |
| V05 | Constant/context output writes absent. Position output forwarded to SV_POSITION without the active backend's screen-space epilogue restoration. | [Output emission](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L986) |
| V06 | Input layout packed from used registers rather than full original declaration/offsets; only one stream; texture register numbers differ from active attrs 9–12. Packed color component ordering needs explicit conversion. | [Layout](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L1096), [attribute constants](tools/xboxrecomp/src/nv2a/nv2a_regs.h) |
| V07 | `Handle >= 0x10000` means programmable shader, but valid FVF texture-size bits also occupy that range. Handle/FVF discrimination is ambiguous. | [Handle selection](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L1424), [FVF bits](tools/xboxrecomp/src/d3d/d3d8_xbox.h#L655) |
| V08 | Fog/back-color semantics do not match fixed-function PS varyings; point size and two-sided colors lack consumers. | [Generated output declarations](tools/xboxrecomp/src/d3d/d3d8_vsh.c), [FF PS input](tools/xboxrecomp/src/d3d/d3d8_shaders.c) |
| V09 | Shader cache keys/variant limits can substitute the wrong layout: variants keyed mainly by texture size, bounded variant fallback uses the first entry, and program cache uses hash without full byte comparison. | [Cache](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L1135), [variants](tools/xboxrecomp/src/d3d/d3d8_vsh.c#L1262) |

These issues do **not** mean A uses this broken decoder. A's GPU shader and CPU helper use the proper word-1 decode, source snapshots and paired ILU rules. Their own remaining gaps are A03 and precision/state-program conformance.

## 6. Wrapper pixel shaders / register combiners

`d3d8_combiners.c` contains substantial HLSL generation, so “pixel shaders are a stub” is stale. It nevertheless does not implement the original encoding and math faithfully enough to claim full Xbox pixel-shader support.

| ID | Gap / defect | Evidence |
| --- | --- | --- |
| P01 | The scalar SetPixelShader token parser uses low-nibble stage count/four-bit modes; actual texture shader program uses five-bit modes. A scalar token is not a complete original Xbox pixel-shader definition. Overrides can replace programmed render-state data. | [Token parser](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L182) |
| P02 | RGB/alpha ICW byte order and OCW AB/CD destination extraction disagree with A's raw NV2A decoder. If these render states contain raw hardware control words, wrong inputs/destinations are selected. | [Parse](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L146), [active HLSL](tools/xboxrecomp/src/video/nv2a_d3d11.c#L671) |
| P03 | Shared/unique constant flags ignored; final combiner constants copied/used from the last general stage instead of independent final constants. SetPixelShaderConstant is also ignored (B07). | [State parse](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L284), [final emission](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L738) |
| P04 | Texture shaders reduced to dimensional sampling. Projection/pass-through/clip/bump/dependent/dot/reflection semantics, PSDOTMAPPING and PSINPUTTEXTURE routing absent. Zero-stage final-only programs cannot be faithfully represented by the forced stage-count path. | [Texture emission](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L547) |
| P05 | r0 is initialized from the whole t0 instead of the actual register initialization; alpha inputs always select .a rather than selectable .b/.a. RGB writes can precede alpha source reads instead of both reading old registers. | [Inputs](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L397), [register init](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L570), [stage writes](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L636) |
| P06 | Mux comparison/AB-CD choice differs; LSB/MSB selection flags ignored. Output range clamping and blue-to-alpha output flags incomplete. | [Mux](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L625) |
| P07 | Input unsigned-invert uses max(1-x,0) instead of 1-saturate(x); final sum inversion/clamp flags not modeled correctly. | [Mapping](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L419), [final sum](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L733) |
| P08 | Fog combiner input uses packed fog color alpha rather than interpolated vertex fog factor. | [Fog input](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L540), [fog application](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L770) |
| P09 | Render-state combiner data alone does not activate the path when the current PS token is zero. | [Preparation guard](tools/xboxrecomp/src/d3d/d3d8_combiners.c#L998) |

A already models source snapshots, blue/alpha selection, shared/unique constants, final constants/flags and mux selection substantially more accurately. Consolidation is a possible future implementation choice; no consolidation was performed here.

## 7. Texture/format translation inventory

### Wrapper format families

The table covers the format families in `d3d8_to_dxgi_format`, software conversion, and their shader interpretation. A DXGI enum mapping alone is not a completed translation. Linear aliases share the status unless stated otherwise.

| Guest family | Current mapping / remaining translation |
| --- | --- |
| A8R8G8B8 / X8R8G8B8 | BGRA/BGRX mapping present; basic upload/swizzle exists |
| A8B8G8R8 / R8G8B8A8 / B8G8R8A8 | Mapping/reordering exists; use pixel tests to validate each guest byte order, not only table checks |
| R5G6B5 / A1R5G5B5 / A4R4G4B4 | Basic packed host mapping exists |
| X1R5G5B5 | Mapped to A1 format without consistently forcing opaque alpha; X-bit is not guest alpha |
| R6G5B5 | Mapped to RGB565 without required 6/5/5→5/6/5 channel repack |
| R5G5B5A1 / R4G4B4A4 | Bit-reorder conversions exist |
| A8 | Explicit white-RGB correction exists in FF and combiner shaders; not a missing translation |
| L8 / L16 / L32 | R-channel mappings without full luminance replication into RGB |
| AL8 / A8L8 / A16L16 / A32L32 | Expansion/channel-pair mapping exists, but sampled luma→RGB and alpha placement are not fully remapped |
| G8B8 / R8B8 | Generic RG mapping does not preserve missing-component and guest channel placement semantics |
| P8 | Palette expansion exists, but converter writes R into the low byte of a BGRA result and B into byte 2; colored palettes can swap R/B. Palette binding/invalidation/lifetime defects below remain |
| YUY2 / UYVY | Software YUV→BGRA conversion exists; full format-specific lock/read-back representation and exact hardware colorimetry not proven |
| DXT1 / DXT3 / DXT5 | BC1/BC2/BC3 mapping exists; decoding helpers and mip upload exist |
| CTX1 / LIN_CTX1 | Explicit BC1 approximation; no correct CTX1 reconstruction |
| DXT3A / DXT5A and linear aliases | Mapped as full BC2/BC3 rather than implementing alpha-only resource layout/sampling semantics |
| DXN and linear alias | BC5 mapping exists; normalization/component interpretation requires conformance tests |
| V8U8 / V16U16 / Q8W8V8U8 / Q16W16V16U16 | SNORM mapping exists; complete signed/filter/swizzle behavior requires pixel tests |
| L6V5U5 | Luminance discarded; signed five-bit components shifted to eight-bit rather than full equivalent normalized expansion |
| X8L8V8U8 | Entire pixel mapped SNORM, losing unsigned luminance/component distinctions |
| LIN_X8L8V8U8 | Declared, but absent from the format mapping, thus goes through unknown-format RGBA fallback |
| V32U32 | Signed integer bytes mapped directly to FLOAT without numeric conversion |
| Q32W32V32U32 | SINT resource with generic floating Texture declarations; typed integer sampling/normalization absent |
| D16 / D24S8 / D24X8 | Host depth mapping exists, but sampleable depth SRV/locking/coherence/DSV creation incomplete |
| F16 | Maps to R16_FLOAT for textures; standalone depth creation substitutes D16; correct Xbox float-depth encoding not implemented |
| F24S8 / D24FS8 | Substitutes D24_UNORM_S8; float-depth interpretation absent |
| D32 fixed | Substitutes D32_FLOAT without numeric fixed→float conversion; wrapper surface DSV creation lacks D32 case |
| G16R16 / A16B16G16R16 | UNORM host mapping/conversion exists; channel order needs format-specific sampled verification |
| G32R32 / A32B32G32R32 | Integer-format payload mapped FLOAT with no numeric conversion; word swapping is not normalization |
| R16F/R32F, G16R16F/G32R32F, A16B16G16R16F/A32B32G32R32F | Host float mappings exist; do not confuse them with the non-F integer families above |
| A2R10G10B10 / A2B10G10R10 | Host packed mapping/selected field swaps exist |
| X2R10G10B10 | Packed X bits not consistently forced to opaque alpha |
| A2W10V10U10 | Signed components substituted with UNORM; signed numeric conversion missing |
| R11G11B10 | FLOAT substitute; exact guest encoding needs validation |
| R10G11B11 | FLOAT 11/11/10 substitute without a correct layout/type conversion |
| INDEX16 / INDEX32 | Host index mappings present |
| Unknown format | Returns RGBA8 and default bpp rather than rejecting unsupported format; downstream UNKNOWN validation can never catch this fallback |

Sources: [mapping table](tools/xboxrecomp/src/d3d/d3d8_resources.c#L39), [bpp/conversion](tools/xboxrecomp/src/d3d/d3d8_resources.c#L244), [software conversions](tools/xboxrecomp/src/d3d/d3d8_resources.c#L560), [shader texture interpretation](tools/xboxrecomp/src/d3d/d3d8_shaders.c#L450).

### A's NV097 texture decoding

A implements the ordinary NV097 luminance/alpha, palette, 16/32-bit color reorder, DXT1/3/5, and linear YUV families directly into BGRA8. In particular, **its luminance replication, R6G5B5 conversion, X1 opacity and palette packing are separate from B's defects**. It has cube-face/mipmap layout and render-target mip-chain reuse. Remaining limitations are the unsupported codes, float/depth precision, no volume depth, incomplete sampling modes, and coherence/cache rules listed in section 1.

`LU_IMAGE_Y16` reinterpretation of a 32-bit color render target has a dedicated host pass, including one-sample MSAA reads to avoid averaging packed bytes. This exists; it is not evidence of general depth-texture or arbitrary alias support. [Y16 shader](tools/xboxrecomp/src/video/nv2a_d3d11.c#L825), [Y16 view](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2526).

## 8. Resource creation, lock/upload/read-back and lifetime

| ID | Path | Missing/partial translation |
| --- | --- | --- |
| R01 | B | VB/IB GetDesc return E_NOTIMPL; texture GetLevelDesc fills only a subset of descriptor fields. Resource pool, priority and preload behavior are simplified/no-op. |
| R02 | B | VB/IB Lock ignore requested size/flags and do not adequately validate offset/range. Unlock uploads whole storage. |
| R03 | B | Texture/cube/volume Lock ignore rectangle/box/flags. CPU shadow data is not refreshed from GPU-rendered RT content. Recorded lock state does not fully associate level/face with the unlock operation. |
| R04 | B | P8 surface subrectangle locks return E_NOTIMPL. Non-P8 surface locks expose host-format staging bytes, without a complete inverse guest-format conversion/reswizzle. |
| R05 | B | Multisample surface Lock resolves directly into STAGING (destination must be DEFAULT single-sample). Unlock attempts resolve from single-sample staging into MSAA (wrong direction). These are invalid host resolve uses, not only a fidelity limitation. |
| R06 | B | Surface RTV/DSV descriptors use 2D/array dimensions rather than multisample dimensions for MSAA resources; CreateView failures are ignored. D32 DSV is not constructed. |
| R07 | B | GetBackBuffer creates a usage-zero surface wrapper, so it has no RTV through the normal surface-create logic. Returning it does not guarantee it can be rebound as a render target. |
| R08 | B | Depth textures use typed depth resources without a typeless resource + sampleable SRV strategy; binding an SRV-less resource can leave stale previous texture state. |
| R09 | B | P8 SetTexture changes palette association without immediate rebake. One texture baked with one palette cannot serve two stages with different palettes correctly. Palette-surface parent pointers do not retain the parent. |
| R10 | B | SetPalette writes the 2D texture `palette` member via a shared cast for volume textures whose structure layout differs; this can overwrite another volume field. |
| R11 | B | Volume texture creation supports real Texture3D and 3D unswizzle, but RT/depth usage is ignored; Pool ignored across resource creation. Cube texture creation exists with faces/mips, so neither cube nor volume creation should be listed as wholly absent. |
| R12 | B | Allocation/conversion/view failures can leave partially constructed or stale-bound resources while success is returned. Unknown format fallback prevents clean unsupported-format reporting. |
| R13 | A | Linear color write-back is flip-based and limited; swizzled/depth output absent. Same-address shape reuse, reinterpretations, cache invalidation and bounds are heuristic rather than a complete shared guest-memory resource model. |

Evidence: [VB/IB methods](tools/xboxrecomp/src/d3d/d3d8_resources.c#L770), [texture descriptors/locks](tools/xboxrecomp/src/d3d/d3d8_resources.c#L1074), [surface locks](tools/xboxrecomp/src/d3d/d3d8_resources.c#L1156), [surface views](tools/xboxrecomp/src/d3d/d3d8_resources.c#L1363), [palette](tools/xboxrecomp/src/d3d/d3d8_resources.c#L1643), [volume creation](tools/xboxrecomp/src/d3d/d3d8_resources.c#L2240).

Host resolve requirements were checked against Microsoft's [ResolveSubresource documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-resolvesubresource): multisampled source, single-sampled DEFAULT destination. A correct lock path needs a DEFAULT resolve texture followed by staging copy; writing back needs a separate legal upload/render strategy.

## 9. Display/video/MMIO and legacy renderer

### PVIDEO and presentation

PVIDEO is not simply missing: the live path snapshots both submission banks, handles STOP/consumption, translates YUY2 pixels, input/output geometry, scaling and color key, and composites through D3D11. [Overlay](tools/xboxrecomp/src/video/fb_present.c#L207), [STOP](tools/xboxrecomp/src/nv2a/nv2a_core.c#L492), [compositor](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3510).

Remaining gaps:

- Overlay accepts one color-format selector corresponding to the implemented YUY2 route; other PVIDEO formats are rejected.
- It preserves one retained overlay image, consuming submitted banks immediately; hardware bank/scanout timing, interrupts and complete overlay color controls are not implemented faithfully.
- The standalone PRAMDAC implementation stores a subset of registers/clock values. B's gamma lookup is attached to B's presentation, while A's compositor has no equivalent gamma-ramp pass. Guest DAC/display-state-to-A gamma translation is not established.
- MMIO opcode handler implements a subset of host load/store/RMW instruction forms and reports decode failures for others. VRAM hook allocates writable pages rather than providing full VRAM/DMA alias semantics. These are graphics-platform compatibility limitations, not xemu CPU execution. [MMIO](tools/xboxrecomp/src/nv2a/nv2a_mmio_hook.c#L386).

### Legacy C

`nv2a_pgraph_d3d11.c` is a different, much smaller translator: fixed XY/UV/color inline-vertex layout, narrow blend mapping, captured/font-atlas/title assumptions, limited primitive handling and no generic original programmable/fixed-function pipeline. Its local method constants include obsolete/inconsistent assignments such as clear 0x01D0 and texture-control 0x1B08, compared with the live NV097 definitions. Replay/test data may match its private assumptions; that does not make it a complete hardware translator.

Treat it as legacy/replay coverage. Do not redirect A into C to “make it real recomp,” and do not use its TODOs as the current MM3 missing-feature list. [Legacy source](tools/xboxrecomp/src/nv2a/nv2a_pgraph_d3d11.c), [replay](tools/xboxrecomp/src/nv2a/nv2a_pb_replay.c#L331), [test dispatch](tools/xboxrecomp/src/nv2a/nv2a_pb_test.c#L286).

## 10. Deliberate enhancements and fidelity differences

These already exist and should be selectable/tested separately from translation correctness:

| Existing change in A | Fidelity implication |
| --- | --- |
| Window-driven render scale, default supersampling, widescreen placement/HUD clustering | Changes projection/placement/resolution; not a missing D3D8 API |
| Global anisotropy and sharp point/font reconstruction | Changes guest filtering/coverage |
| Completion of short texture mip chains | Synthesizes data absent from the guest resource |
| Generated render-target mips for reductions and guest-pixel tap averaging | Changes the sampling footprint for upscale stability |
| MSAA alpha-test → coverage treatment | Changes native discard/coverage behavior |
| Half-pixel/edge snapping heuristics | Title-oriented coordinate adjustment rather than a universally verified raster rule |
| Host BGRA8 color/depth substitutes and host sample patterns | Different arithmetic/storage/resolve precision |

Evidence: [sampling HLSL](tools/xboxrecomp/src/video/nv2a_d3d11.c#L580), [host mip generation](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2000), [reduction pipeline](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2496), [HUD split](tools/xboxrecomp/src/video/nv2a_d3d11.c#L2890), [display configuration](tools/xboxrecomp/src/video/nv2a_d3d11.c#L3680).

An accurate comparison should pin native aspect/scale and disable enhancements where available. Any remaining behavior without an off switch should be recorded as a comparison limitation, not silently assumed faithful.

## 11. What is already translated

Avoid spending work reimplementing these because an old note called them absent:

- **A:** raw NV2A programmable vertex instructions on the GPU; 192 constants; 16 fetched/current attributes; indexed/nonindexed/inline/immediate submission; fan/quad/polygon conversion; normal blend/depth/stencil/color masks/culling/flat shade/fill offset; W-depth; fog; alpha test; substantial register-combiner/final-combiner semantics; palettes; Morton texture unswizzle; DXT decode; cube maps/mips; render-target feedback copies and rendered mip-chain reuse; Y16 reinterpretation; rectangular masked clears; z-pass queries; linear color write-back; PVIDEO composition; window resizing and host presentation.
- **B:** texture/cube/volume creation, buffer upload, selected format conversion, basic fixed-function transforms/lighting/fog/alpha test/specular/texture operations, basic depth/stencil/blend states, gamma lookup, and A8 white-RGB semantics.

“Implemented” means code exists for these behaviors, not that every value/edge case has passed hardware comparison.

## 12. Source coverage and validation boundaries

| Code area | Audit coverage |
| --- | --- |
| `src/host_graphics.c`, `src/main.c`, `src/recomp_manual.c` | Initialization and graphics/MMIO/manual compatibility integration |
| Root and toolkit CMake source lists | Which implementations are compiled/linked and separate backend boundaries |
| `src/d3d/d3d8_device.c` | Device/factory API, vtable, binding, draw conversion, target creation, presentation and stubs |
| `d3d8_states.c` | Blend/depth/stencil/raster/sampler consumers versus stored state |
| `d3d8_shaders.c` | Fixed-function HLSL, input layouts, dimension variants and constant upload |
| `d3d8_vsh.c` | Decoder/opcode emitter, constants, layouts, handles and cache |
| `d3d8_combiners.c/.h` | Token/state parse, generated sampling/combiner HLSL and cache/activation |
| `d3d8_resources.c` | Format/bpp/conversion, buffer/texture/cube/volume/surface creation, locks and palette helpers |
| `d3d8_xbox.h`, `d3d8_internal.h`, `d3d8_fvf.h`, `d3d8_swizzle.h` | Declared API/state/format surface, storage/layout and shared conversion helpers |
| `d3d8_gamma.c` | Existing wrapper gamma path and scope |
| `kernel/xbox_memory_layout.c`, `nv2a_pb_scan.c` | DMA submission, command walk/calls/jumps/limits, executor connection |
| `kernel/nv2a_pb_exec.c` | Live method/range handlers, state transfer, topology/batch routing, software-fallback boundary and completion |
| `kernel/nv2a_vsh_interp.c`, `nv2a_combiner.c` | CPU helper/reference semantics used alongside the GPU path; not evidence of full software-renderer conformance |
| `video/nv2a_d3d11.c/.h`, `fb_present.c/.h` | Actual MM3 GPU shader/resource/state/clear/coherence/overlay/presentation translation |
| `nv2a_core.c`, `nv2a_mmio_hook.c`, `nv2a_regs.h` | Register definitions, MMIO/PVIDEO behavior and legacy dispatch boundary |
| `nv2a_pgraph_d3d11.c/.h`, `nv2a_pb_replay.c`, `nv2a_pb_test.c` | Legacy/replay distinction and limitations |
| `tests/d3d8_smoke` | Existing test targets and coverage limits |
| `d3d8_gl.c`, non-Windows D3D11 stubs | Separate platform implementation; excluded from Windows D3D11 feature findings |
| Generated recomp output, assets, xemu binaries | Not a substitute for translator source; no execution/asset modifications performed |

Existing tests include pure format/swizzle/conversion smoke coverage, bound depth/stencil/blend WARP checks, A8 sampling checks and gamma tests. They do not prove A's full GPU command pipeline, raw shader conformance, every format's sampled pixels, guest-memory ordering, or MM3 gameplay correctness. Existing tests were inspected, not run, because this is a documentation-only read-only investigation.

The scan report has an additional diagnostic defect: several name-table labels are stale/wrong (for example 0x1808 labeled INLINE_ARRAY instead of ARRAY_ELEMENT32, transform program/constants reversed). Numeric methods/actual dispatch should be used as evidence until labels are corrected. [Name table](tools/xboxrecomp/src/kernel/nv2a_pb_scan.c#L80).

This report covers source-visible missing and incomplete translation families across the current rendering implementations. It is **not a proof that every undocumented NV2A bit or XDK ABI has been exhaustively specified**. Unsupported values can hide behind default branches; hardware/xemu comparisons are needed to close precision, special texture-mode and timing uncertainties. No percentage of “complete” is justified by this source audit alone.

## 13. Suggested order of work — no changes made

1. **Fix unconditional correctness defects in A:** sampler identity/cache saturation, signed/mixed-constant blend mapping, small primitive guards, same-frame texture invalidation and capacity/error reporting. Add focused reproductions for each behavior before claiming it fixed.
2. **Trace actual MM3 missing methods through menu/loading/driving and effects:** record numeric method/state/format values and parser health. Separate real unsupported operations from harmless setup/no-ops. Do not infer usage from a register header or old replay.
3. **Close exercised A resource gaps:** depth/swizzled coherence, sampleable depth/shadow modes, exact format precision and required synchronization/object dispatch. Add fixed T&L/volume textures only when exercised or when general toolkit completeness is the objective.
4. **Repair B as its own compatibility layer:** correct raw vertex/combiner decoding or reuse the verified semantics, expose working shader APIs, finish state/format/lock/target handling and lifetime validation. Those repairs are not a shortcut to fixing A.
5. **Prove pixels and gameplay:** native-scale/aspect captures versus the reference, then enhancements; menu, loading, driving, reflections/water/glare, fog/shadows, HUD/font, movies and DLC where relevant. Track GPU debug-layer errors, resource read-back and frame-time stability. An intro movie, draw count, successful build or format-table test is not completion.
