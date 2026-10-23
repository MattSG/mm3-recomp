# NV2A combiner and vertex interface ownership

Reviewed full nv2a_combiner.c, nv2a_combiner.h and nv2a_vsh_interp.h at fork
693fb082cd1f97b43cba1534c0948ccbb81c3acd. The combiner consumes caller-provided register programs and
pixel values; optimized paths match register encodings and retain a general
interpreter fallback. No embedded title image/program payload was found.
Literal program encodings here are generic semantic dispatch, not a title
address table. This finding does not prove all fast paths are equivalent.

Title rendering history, old executor narrative, performance observations and
coordinate-space claims moved to legacy_nv2a_interface_notes.md. Vertex output
is documented as raw oPos/program values, matching the inspected return-copy
path. The input count comment now agrees with the 16-element declaration.
The rest of the vertex interpreter body was not reviewed in this checkpoint.

Non-comment token equality passes in all three edited files. The existing
combiner test covers three analytical programs (modulation, final interpolation,
expanded dot product). It passes default and forced RECOMP_RC_GENERIC paths on
Windows and Linux. These are software expected-value checks, not captured
hardware comparisons or complete numerical/fast-path/renderer certification.
No new gameplay or backend fidelity result is claimed.
