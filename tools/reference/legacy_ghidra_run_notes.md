# Archived Ghidra run notes

Captured addresses and function counts from prior analysis runs. These are
historical evidence, not generic toolkit defaults or current coverage proof.

## tools/ghidra_naming/ghidra_scripts/ClassifyTargets.java

```text
 *   CT 0022e280 block=.text func=- INSN:MOV EAX,[0x005bb930]
 *   CT 00194890 block=.text func=FUN_00194890@00194890(ENTRY) INSN:PUSH -0x1
```

## tools/ghidra_naming/ghidra_scripts/SeedFunctions.py

```text
# The project's own disassembler already identified 22,178 function starts in
# tools/disasm/output/functions.json. Seeding those exact addresses as functions
# lets Function ID + the decompiler operate on properly-bounded bodies at the
# addresses the recompiler keys on, maximizing name-transfer overlap.
```
