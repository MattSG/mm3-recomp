# Framebuffer presenter ownership

Full fb_present.c source read at fork fe644314d7283f72a021381ec7e5cbebdad71b84. Display/scanout state,
PVIDEO register snapshots, retained overlay pixels, host overlay callbacks,
input/window state and optional frame diagnostics are generic presentation
mechanisms. Caller callbacks hold application policy; no title ID, captured
frame payload or game address table is embedded here.

PVIDEO validation used a fixed 64 MB retail RAM bound even though the runtime
supports configurable installed RAM. It now uses g_xbox_total_ram, separately
from extra guest backing. The source retains existing mapping checks and bank
consumption behavior. No parallel RAM configuration mechanism was added.

An actual-presenter fixture mocks only register snapshots/consumption, window
and guest backing origin. Old code fails admission above 64 MB. Final Windows
and Linux tests admit a packet ending exactly at configured 128 MB, reject the
same mapped pixels under 64 MB, reject one byte beyond 128 MB and check bank
consumption/pixel retention. Both D3D11 and SDL GPU game builds pass.

Remaining limits: snapshot-to-consume ordering, source mapping races, malformed
geometry before optional capture, retained allocations/lifetime, window-state
publication and diagnostic initialization need wider correctness review. No
hardware PVIDEO fidelity or live movie/gameplay equivalence is certified.
