# Historical function-identification example

From fork 1593662 tools/README.md. Captured addresses and advice are historical, not current acceptance evidence.

**Output:** Annotated function database with categories:

```
0x00011000  CRT     _mainCRTStartup
0x00011240  GAME    resource_loader
0x000636D0  GAME    physics_force_apply
0x001DD910  RW      RwCameraBeginUpdate
0x0034D530  RW      renderware_main_render (79KB!)
```

**Why this matters:** Knowing which functions are CRT vs RenderWare vs game code helps you prioritize. CRT functions usually "just work" in recompiled form. RenderWare functions may need manual overrides. Game functions need the most attention.
