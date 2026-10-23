# Remaining SA arithmetic review

This is a source inventory, not a conformance claim. The original DSP/audio acceptance objective remains open.

Current shared paths cover tested word/long moves, selected ADD/SUB/CMP forms, ADC/SBC, word logic, rotations, and logical/arithmetic shifts. Tests cover specific operand boundaries and formats, not arbitrary-program correctness.

The current multiplication helper still reads signed bit 23 and multiplies two 24-bit operands into a 56-bit result. It has no SA argument. Direct MPY/MAC callers therefore require a shared sixteen-bit multiplication correction; accumulated forms also need continuous forty-bit add/subtract. Masking input bytes alone cannot repair the result's physical word gap. Validate signed extremes, negative-product selection, rounding, and accumulation before claiming this family works.

Shifted ADDL/ADDR/SUBL/SUBR retain direct full-width helpers. ABS/NEG and magnitude comparisons require width-aware significant-word packing and signed-minimum handling. Immediate operands, bit parsing, normalization, count-source fields, and mode-change hazards require separate manual review.

Two wrappers intentionally call old helpers for normal-mode fallback. Do not treat every inventory row as a confirmed defect. Conversely, no NULL decoder handlers and green frame replay do not prove SA conformance.

Generated caller inventory follows; line numbers identify the inspected source snapshot and may move. Source SHA-256: `dbce744f58c115e66a7f5855e469efff88982dcfb3578b2a1fc08f80dbe60f88`.

| Handler | Line | Direct helpers |
|---|---:|---|
| `emu_abs_a` | 320 | `dsp_abs56` |
| `emu_abs_b` | 342 | `dsp_abs56` |
| `emu_addl_b_a` | 868 | `dsp_add56`, `dsp_asl56` |
| `emu_addl_a_b` | 893 | `dsp_add56`, `dsp_asl56` |
| `emu_addr_b_a` | 918 | `dsp_add56`, `dsp_asr56` |
| `emu_addr_a_b` | 943 | `dsp_add56`, `dsp_asr56` |
| `emu_arithmetic_shift` | 1064 | `dsp_asl56`, `dsp_asr56` |
| `emu_cmpm_b_a` | 1397 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_a_b` | 1420 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_x0_a` | 1443 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_x0_b` | 1466 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_y0_a` | 1489 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_y0_b` | 1512 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_x1_a` | 1535 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_x1_b` | 1558 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_y1_a` | 1581 | `dsp_abs56`, `dsp_sub56` |
| `emu_cmpm_y1_b` | 1604 | `dsp_abs56`, `dsp_sub56` |
| `emu_mac_p_x0_x0_a` | 1815 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x0_x0_a` | 1837 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x0_x0_b` | 1858 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x0_x0_b` | 1880 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y0_y0_a` | 1902 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y0_y0_a` | 1924 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y0_y0_b` | 1945 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y0_y0_b` | 1967 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x1_x0_a` | 1989 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x1_x0_a` | 2011 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x1_x0_b` | 2033 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x1_x0_b` | 2055 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y1_y0_a` | 2077 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y1_y0_a` | 2099 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y1_y0_b` | 2121 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y1_y0_b` | 2143 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x0_y1_a` | 2165 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x0_y1_a` | 2187 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x0_y1_b` | 2209 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x0_y1_b` | 2231 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y0_x0_a` | 2253 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y0_x0_a` | 2275 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y0_x0_b` | 2297 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y0_x0_b` | 2319 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x1_y0_a` | 2341 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x1_y0_a` | 2363 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_x1_y0_b` | 2385 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_x1_y0_b` | 2407 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y1_x1_a` | 2429 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y1_x1_a` | 2451 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_p_y1_x1_b` | 2473 | `dsp_add56`, `dsp_mul56` |
| `emu_mac_m_y1_x1_b` | 2495 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x0_x0_a` | 2517 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x0_x0_a` | 2541 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x0_x0_b` | 2564 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x0_x0_b` | 2588 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y0_y0_a` | 2612 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y0_y0_a` | 2636 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y0_y0_b` | 2659 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y0_y0_b` | 2683 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x1_x0_a` | 2707 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x1_x0_a` | 2731 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x1_x0_b` | 2755 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x1_x0_b` | 2779 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y1_y0_a` | 2803 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y1_y0_a` | 2827 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y1_y0_b` | 2851 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y1_y0_b` | 2875 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x0_y1_a` | 2899 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x0_y1_a` | 2923 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x0_y1_b` | 2947 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x0_y1_b` | 2971 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y0_x0_a` | 2995 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y0_x0_a` | 3019 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y0_x0_b` | 3043 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y0_x0_b` | 3067 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x1_y0_a` | 3091 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x1_y0_a` | 3115 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_x1_y0_b` | 3139 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_x1_y0_b` | 3163 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y1_x1_a` | 3187 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y1_x1_a` | 3211 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_p_y1_x1_b` | 3235 | `dsp_add56`, `dsp_mul56` |
| `emu_macr_m_y1_x1_b` | 3259 | `dsp_add56`, `dsp_mul56` |
| `emu_mpy_p_x0_x0_a` | 3290 | `dsp_mul56` |
| `emu_mpy_m_x0_x0_a` | 3304 | `dsp_mul56` |
| `emu_mpy_p_x0_x0_b` | 3318 | `dsp_mul56` |
| `emu_mpy_m_x0_x0_b` | 3332 | `dsp_mul56` |
| `emu_mpy_p_y0_y0_a` | 3347 | `dsp_mul56` |
| `emu_mpy_m_y0_y0_a` | 3361 | `dsp_mul56` |
| `emu_mpy_p_y0_y0_b` | 3375 | `dsp_mul56` |
| `emu_mpy_m_y0_y0_b` | 3389 | `dsp_mul56` |
| `emu_mpy_p_x1_x0_a` | 3403 | `dsp_mul56` |
| `emu_mpy_m_x1_x0_a` | 3417 | `dsp_mul56` |
| `emu_mpy_p_x1_x0_b` | 3431 | `dsp_mul56` |
| `emu_mpy_m_x1_x0_b` | 3445 | `dsp_mul56` |
| `emu_mpy_p_y1_y0_a` | 3459 | `dsp_mul56` |
| `emu_mpy_m_y1_y0_a` | 3473 | `dsp_mul56` |
| `emu_mpy_p_y1_y0_b` | 3487 | `dsp_mul56` |
| `emu_mpy_m_y1_y0_b` | 3501 | `dsp_mul56` |
| `emu_mpy_p_x0_y1_a` | 3515 | `dsp_mul56` |
| `emu_mpy_m_x0_y1_a` | 3529 | `dsp_mul56` |
| `emu_mpy_p_x0_y1_b` | 3543 | `dsp_mul56` |
| `emu_mpy_m_x0_y1_b` | 3557 | `dsp_mul56` |
| `emu_mpy_p_y0_x0_a` | 3571 | `dsp_mul56` |
| `emu_mpy_m_y0_x0_a` | 3585 | `dsp_mul56` |
| `emu_mpy_p_y0_x0_b` | 3599 | `dsp_mul56` |
| `emu_mpy_m_y0_x0_b` | 3613 | `dsp_mul56` |
| `emu_mpy_p_x1_y0_a` | 3627 | `dsp_mul56` |
| `emu_mpy_m_x1_y0_a` | 3641 | `dsp_mul56` |
| `emu_mpy_p_x1_y0_b` | 3655 | `dsp_mul56` |
| `emu_mpy_m_x1_y0_b` | 3669 | `dsp_mul56` |
| `emu_mpy_p_y1_x1_a` | 3683 | `dsp_mul56` |
| `emu_mpy_m_y1_x1_a` | 3697 | `dsp_mul56` |
| `emu_mpy_p_y1_x1_b` | 3711 | `dsp_mul56` |
| `emu_mpy_m_y1_x1_b` | 3725 | `dsp_mul56` |
| `emu_mpyr_p_x0_x0_a` | 3739 | `dsp_mul56` |
| `emu_mpyr_m_x0_x0_a` | 3754 | `dsp_mul56` |
| `emu_mpyr_p_x0_x0_b` | 3769 | `dsp_mul56` |
| `emu_mpyr_m_x0_x0_b` | 3784 | `dsp_mul56` |
| `emu_mpyr_p_y0_y0_a` | 3800 | `dsp_mul56` |
| `emu_mpyr_m_y0_y0_a` | 3815 | `dsp_mul56` |
| `emu_mpyr_p_y0_y0_b` | 3830 | `dsp_mul56` |
| `emu_mpyr_m_y0_y0_b` | 3845 | `dsp_mul56` |
| `emu_mpyr_p_x1_x0_a` | 3860 | `dsp_mul56` |
| `emu_mpyr_m_x1_x0_a` | 3875 | `dsp_mul56` |
| `emu_mpyr_p_x1_x0_b` | 3890 | `dsp_mul56` |
| `emu_mpyr_m_x1_x0_b` | 3905 | `dsp_mul56` |
| `emu_mpyr_p_y1_y0_a` | 3920 | `dsp_mul56` |
| `emu_mpyr_m_y1_y0_a` | 3935 | `dsp_mul56` |
| `emu_mpyr_p_y1_y0_b` | 3950 | `dsp_mul56` |
| `emu_mpyr_m_y1_y0_b` | 3965 | `dsp_mul56` |
| `emu_mpyr_p_x0_y1_a` | 3980 | `dsp_mul56` |
| `emu_mpyr_m_x0_y1_a` | 3995 | `dsp_mul56` |
| `emu_mpyr_p_x0_y1_b` | 4010 | `dsp_mul56` |
| `emu_mpyr_m_x0_y1_b` | 4025 | `dsp_mul56` |
| `emu_mpyr_p_y0_x0_a` | 4040 | `dsp_mul56` |
| `emu_mpyr_m_y0_x0_a` | 4055 | `dsp_mul56` |
| `emu_mpyr_p_y0_x0_b` | 4070 | `dsp_mul56` |
| `emu_mpyr_m_y0_x0_b` | 4085 | `dsp_mul56` |
| `emu_mpyr_p_x1_y0_a` | 4100 | `dsp_mul56` |
| `emu_mpyr_m_x1_y0_a` | 4115 | `dsp_mul56` |
| `emu_mpyr_p_x1_y0_b` | 4130 | `dsp_mul56` |
| `emu_mpyr_m_x1_y0_b` | 4145 | `dsp_mul56` |
| `emu_mpyr_p_y1_x1_a` | 4160 | `dsp_mul56` |
| `emu_mpyr_m_y1_x1_a` | 4175 | `dsp_mul56` |
| `emu_mpyr_p_y1_x1_b` | 4190 | `dsp_mul56` |
| `emu_mpyr_m_y1_x1_b` | 4205 | `dsp_mul56` |
| `emu_neg_a` | 4220 | `dsp_sub56` |
| `emu_neg_b` | 4244 | `dsp_sub56` |
| `emu_subl_a` | 4972 | `dsp_asl56`, `dsp_sub56` |
| `emu_subl_b` | 4997 | `dsp_asl56`, `dsp_sub56` |
| `emu_subr_a` | 5022 | `dsp_asr56`, `dsp_sub56` |
| `emu_subr_b` | 5049 | `dsp_asr56`, `dsp_sub56` |
| `emu_max` | 5188 | `dsp_sub56` |
| `emu_add_x` | 5919 | `dsp_add56` |
| `emu_cmp_imm` | 6685 | `dsp_sub56` |
| `emu_cmp_long` | 6714 | `dsp_sub56` |
| `emu_cmpu` | 6744 | `dsp_sub56` |
| `emu_dec` | 6794 | `dsp_sub56` |
| `emu_div` | 6832 | `dsp_add56`, `dsp_asl56`, `dsp_sub56` |
| `emu_normf` | 7153 | `dsp_asl56`, `dsp_asr56` |
| `emu_inc` | 7208 | `dsp_add56` |
| `emu_multiply_result` | 8166 | `dsp_add56` |
| `emu_mpyi` | 8196 | `dsp_mul56` |
| `emu_multiply_mixed` | 8223 | `dsp_asr56` |
| `emu_norm` | 8254 | `dsp_asl56`, `dsp_asr56` |
| `emu_sub_x` | 8426 | `dsp_sub56` |

Update: unrounded parallel MPY now routes through emu_multiply; the inventory above is the earlier hashed snapshot. Rounded, accumulated, and immediate multiply paths still require correction and validation.

Update: rounded parallel MPYR also routes through emu_multiply; accumulated and immediate paths remain open.

Update (closure): all handlers in the inventory now route through SA-aware wrappers or dedicated SA paths.
