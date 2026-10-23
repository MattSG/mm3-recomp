# Historical D3D8 device/backend introductions

Original source commentary from fork 3e2de83, preserved as bring-up history.
These paragraphs do not establish current coverage or a required title engine.

## d3d8_device.c

```c
/**
 * D3D8→D3D11 Compatibility Device Implementation
 *
 * Implements the Xbox D3D8 IDirect3DDevice8 interface using D3D11.
 * The game's translated RenderWare code calls D3D8 methods through
 * COM vtables; this layer translates those calls to D3D11 equivalents.
 *
 * Architecture:
 * - D3D11 device and swap chain created during initialization
 * - Render state tracking: D3D8 states mapped to D3D11 state objects
 * - Texture/buffer management: D3D8 resource handles wrap D3D11 resources
 * - Fixed-function pipeline: emulated via D3D11 shaders (the Xbox D3D8
 *   pipeline is configurable but not fully programmable)
 *
 * Build: Requires Windows SDK with d3d11.h and dxgi.h
 */
```

## d3d8_gl.c

```c
/*
 * d3d8_gl.c - OpenGL 3.3 backend for the D3D8 compat layer (Linux)
 *
 * The Windows build keeps using D3D11 (d3d8_device.c / d3d8_resources.c /
 * d3d8_states.c / d3d8_combiners.c / d3d8_vsh.c / d3d8_shaders.c). The
 * CMake split picks this file on POSIX.
 *
 * This is the FIRST cut of the real OpenGL backend:
 *   - SDL3 window + GL 3.3 core context
 *   - All Xbox D3D8 COM interfaces wired up (every vtable slot has a body)
 *   - Real Clear, Present, BeginScene/EndScene, viewport, scissor
 *   - Render-state caching with a small "apply" path for depth/blend/cull/etc.
 *   - CreateTexture / CreateVertexBuffer / CreateIndexBuffer with shadow
 *     memory + a GL handle ready for upload
 *   - DrawPrimitive(UP) / DrawIndexedPrimitive(UP) with a fixed GLSL program
 *     supporting position + diffuse + UV (the FVF set the recompiled RW
 *     driver actually emits at this point)
 *
 * Next iterations: real texture-format mapping, FVF-driven input layouts
 * beyond pos/diffuse/UV, Xbox VSH bytecode → GLSL, register-combiner
 * pixel programs → GLSL fragment.
 */
```

Replacement comments describe the generic object-interface boundary and leave
specific feature coverage to implementation/tests. A string-aware C lexer
confirms executable tokens are identical in both complete source files.
