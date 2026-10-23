# Shared MMIO decoder ownership and encoding frontier

Full mmio_decode.h source reviewed at fork ae79ef6118a9419fbdf552ba50423b4f8c05a61a. Callers include the
network and OHCI fault handlers; access callbacks and device offsets are
caller-provided. No title address table or captured instruction stream exists
in the decoder. Historical extraction narrative moved to legacy_mmio_decoder_notes.md.

Corrected shared mechanisms:

- No-base SIB addressing consumes its disp32, including REX.B forms.
- C7 consumes imm16 for word operands, imm32 otherwise, sign-extending for
  64-bit operands. REX.W overrides operand-size 66.
- Invalid C6/C7 extensions and register-only forms are rejected before device
  callbacks; unsupported forms leave RIP unchanged.

Hand-encoded actual-header cases reproduce six failures before correction.
Existing and new checks pass Windows/Linux; a portable CMake target replaces
the test's Windows-only header/type dependency. Initial Linux compilation found
DWORD64 absent in the portable vocabulary; test casts now use uint64_t.
Both game builds pass. No live-device/hardware comparison was performed.

Unresolved generic defects from source review: legacy high-byte registers are
not modeled; flags are incomplete and logical RMW operations do not update
flags; MOVZX ignores a word destination; general register-only ModRM forms and
prefix/instruction byte bounds need validation. Source remains needs_generic_fix.
These are runtime mechanism gaps, not application-specific workaround needs.
