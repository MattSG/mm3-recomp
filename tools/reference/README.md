# Retired title-specific runtime material

These files moved out of XboxRecomp during the ownership audit. They are not
compiled or used by MM3:

- `legacy_nv2a/`: removed. It was a captured-menu frontend prototype with empty fixtures.
- `legacy_kernel_imports.h`: old reference title's import order, formerly used
  when no XBE had been loaded. Runtime imports now come from each XBE.

- `legacy_section_layouts.json`: removed implicit reference-image layouts.
- `legacy_xbox_memory.h`, `legacy_kernel_stubs.h`: obsolete port-side memory
  constants and success-stub templates, superseded by runtime interfaces.

They retain provenance for the removal; they are not fallback implementations.
