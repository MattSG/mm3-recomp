# Retired captured-menu prototype

Moved unchanged from XboxRecomp a18f4eb `src/nv2a` during the ownership audit.
Reference source only; not built or used by MM3. Relative includes reflect the
original location and are not a supported build interface.

This is a Burnout 3 frontend prototype: fixed menu labels, guest texture and
pushbuffer addresses, captured font mapping, chyron animation and empty capture
placeholders. It is not a generic GPU implementation. No MM3 caller initializes
its translator or calls its replay/test activation APIs. The generic runtime
continues to use its actual pushbuffer executor and D3D11/SDL GPU renderers.

Retained here for provenance. Do not reactivate it as a fallback renderer.
A real consumer should use the runtime's command executor and title-owned hooks
with its own assets.
