# Historical NV2A interface narratives

Archived introductions, performance observations and coordinate-space claims.
Current interfaces describe mechanisms and raw program outputs; these notes
are not runtime fidelity or performance proof.

## tools\xboxrecomp\src\kernel\nv2a_combiner.h

```text
 * Every pixel the NV2A writes goes through its register combiners: up to
 * eight general stages and a final stage that mix the vertex colours, the
 * four texture results, two constants per stage and fog. D3D's fixed-function
 * texture stages compile to them too, so a title never draws without them.
 * The executor used to stand in with "texel * diffuse" from stage 0, which is
 * right for a menu quad and wrong for anything that mixes two textures --
 * Burnout 3 composites its whole 3D scene into the frame that way.
 *
```

## tools\xboxrecomp\src\kernel\nv2a_combiner.c

```text
/* D3DCOLOR to floats, memoised: a program's constants are the same for
 * every pixel of a batch, and unpacking them per pixel was a measurable
 * share of the executor. */
```

## tools\xboxrecomp\src\kernel\nv2a_vsh_interp.h

```text
 * The executor (nv2a_pb_exec.c) could only draw vertices that arrive already
 * in screen space. A title running vertex programs -- every 3D scene on the
 * Xbox -- sends model-space positions and lets the GPU transform them, so
 * those batches were counted and skipped. This runs the uploaded program on
 * the CPU, one vertex at a time, and hands back the outputs the rasteriser
 * needs.
 *
```

## tools\xboxrecomp\src\kernel\nv2a_vsh_interp.h

```text
/* What one vertex comes out as. Position is whatever the program wrote to
 * oPos: on Xbox D3D that is already screen space -- x, y in pixels, z in the
 * depth buffer's range -- because the runtime appends the viewport transform
 * and the divide by w to every shader, with w left as the clip-space w. */
```
