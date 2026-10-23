# Historical object-translator encoding commentary

The old field layout below was contradicted by the independent wire probe.
It is preserved as history, not an encoding specification.

```c
/* ================================================================
 * NV2A VS Instruction Encoding (128 bits = 4 DWORDs)
 *
 * Each instruction is composed of 4 x 32-bit words:
 *
 * Word 0 (ILU/misc):
 *   [31:29] - Reserved / type
 *   [28:25] - ILU opcode (4 bits)
 *   [24:21] - MAC opcode (4 bits)
 *   [20:13] - Const index (8 bits) - constant register selector
 *   [12:9]  - Input reg (v#) (4 bits) - which v0-v15 register
 *   [8]     - Source A negate
 *   [7:4]   - Source A register type + index (mux field A)
 *   [3:0]   - Source A swizzle X component (2 bits in combined field)
 *
 * Word 1 (Source A/B):
 *   [31:24] - Source A swizzle (Y, Z, W components) + remaining bits
 *   [23:16] - Source B register select + negate + type
 *   [15:0]  - Source B swizzle + remaining fields
 *
 * Word 2 (Source C / MAC dest):
 *   [31:16] - Source C register select + negate + swizzle
 *   [15:12] - MAC dest temp register index
 *   [11:8]  - MAC dest write mask (xyzw)
 *   [7:3]   - MAC dest output register mux
 *   [2:0]   - Remaining source C bits
 *
 * Word 3 (ILU dest / final):
 *   [31:28] - ILU dest temp register index
 *   [27:24] - ILU dest write mask (xyzw)
 *   [23:19] - ILU dest output register mux
 *   [18:1]  - Reserved / other fields
 *   [0]     - Final instruction flag (1 = last instruction in program)
 *
 * NOTE: The exact bit layout follows the xemu/envytools conventions.
 * The fields below are extracted using shift-and-mask operations
 * matching the NV2A hardware encoding.
 * ================================================================ */


```
