/* Historical independent wire reproducer: corrected decoder should pass.
 * Uses the production parser and existing host test stubs. Generic wire
 * regression coverage now also lives in tests/d3d8_smoke/test_vsh.c. */
#define main ownership_existing_vsh_test_main
#include "../xboxrecomp/tests/d3d8_smoke/test_vsh.c"
#undef main
int main(void)
{
    /* MOV oPos.xyzw, c[191].xyzw; final. Literal words derive from pinned
     * xemu field definitions, not the production parser's macros. */
    const DWORD wire[4] = {0, 0x0037E01Bu, 0x0C000000u, 0x0000F801u};
    NV2AVshProgram *program = calloc(1, sizeof(*program));
    if (!program) return 2;
    d3d8_vsh_parse(wire, 1, program);
    const NV2AVshInstruction *i = &program->insns[0];
    int ok = program->length == 1 && i->mac_op == NV2A_VSH_MAC_MOV &&
        i->const_index == 191 && i->mac_src[0].reg_type == NV2A_VSH_REG_CONST &&
        i->mac_src[0].reg_index == 191 && i->mac_src[0].swizzle.x == 0 &&
        i->mac_src[0].swizzle.y == 1 && i->mac_src[0].swizzle.z == 2 &&
        i->mac_src[0].swizzle.w == 3 && i->mac_dst.output_reg == NV2A_VSH_OUT_POS &&
        i->is_final;
    printf("wire MOV c191: mac=%d constant=%d source_type=%d source_index=%d output=%d final=%d; %s\n",
        i->mac_op,i->const_index,i->mac_src[0].reg_type,i->mac_src[0].reg_index,
        i->mac_dst.output_reg,i->is_final,ok ? "PASS" : "FAIL");
    free(program);
    return ok ? 0 : 1;
}
