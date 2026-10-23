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


## Register lanes and status flags

Fork eb5e2e4 shares register read/write helpers across supported
operations. AH/CH/DH/BH without REX map to bits 8..15; REX selects low byte
registers. Word writes preserve upper bits; DWORD writes clear them. MOVZX
uses its actual destination width. Writes to devices are masked to operand size.

Logical operations now set ZF/SF/PF and clear CF/OF; undefined AF is preserved.
CMP also sets signed overflow and auxiliary carry while preserving unrelated
context flags. Eight added checks fail on prior code and pass after correction.
Twenty further CMP edge cases cover all four widths, carry, signed overflow,
auxiliary carry, zero and parity; CMP issues no writes. Actual-header CTests
pass Windows/Linux and both game builds pass. These are hand-encoded expected
semantics, not native CPU differential or live-device tests.

The previously recorded lane/flag/MOVZX defects are addressed. General ModRM
validation and prefix/instruction byte bounds remain open; needs_generic_fix
stays in place for the shared source.


## Bounded validation and checked instruction fetch

Fork 8ffa9fe adds a bounded caller-owned snapshot decoder. Before
callbacks/context mutation it verifies the supported opcode, memory ModRM,
SIB/displacement/immediate length and 15-byte architectural limit. REX state
is discarded when a later legacy prefix makes it ineffective. Unsupported
encodings remain rejected rather than guessed.

The existing fault-handler entry point snapshots up to 15 bytes through the
platform ReadProcessMemory primitive. If a bulk read is refused entirely,
byte reads recover the readable prefix. A valid instruction ending at a guard
page remains serviceable; incomplete/unreadable instructions return failure.
LastError from fetching is restored before device execution.

Prior code fails 42 assertions covering 13 register-only forms and an overlong
encoding. Final Windows/Linux checks pass those cases, every truncated prefix
of four supported encodings, legacy-after-REX ordering, a maximum-length form,
valid/truncated page-end instructions, unreadable memory and LastError
preservation. Both game builds pass. Test target links the existing platform
primitive; no second host memory reader was introduced.

The recorded encoding/lane/flag/bounds defects are addressed for this supported
subset. Source ownership now retains the generic mechanism. No full x86 ISA,
native CPU differential, live device, macOS, concurrent code mutation or
performance certification is claimed; the broader fork audit is incomplete.
