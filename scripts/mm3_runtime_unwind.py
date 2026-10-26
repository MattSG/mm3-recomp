"""Post-translation rewrites for code the lifter cannot express directly."""


def unwind_after_call(body, target):
    """Return from the caller when the call to target discards its return address.

    Some x87 helpers pop the address their caller pushed and return straight
    through the dispatch entry that called them. A C call would resume after
    the call and pop again, so the emitted call is followed by a check that
    the helper left ESP four bytes higher, and returns when it did. Returns
    the rewritten body; raises ValueError when the call is absent."""
    marker = f"RECOMP_ABI_CALL(0x{target:08X}u, sub_{target:08X})"
    lines = body.splitlines()
    found = False
    for index, line in enumerate(lines):
        if marker in line:
            lines[index] = ("    { uint32_t math_sp = esp;\n" + line +
                            "\n      if (esp == math_sp + 4u) return; }")
            found = True
    if not found:
        raise ValueError(f"no call to 0x{target:08X} in body")
    return "\n".join(lines)
