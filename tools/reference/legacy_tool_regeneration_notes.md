# Historical tool regeneration notes

Source: XboxRecomp a35977bda89c49c107ac3d30aff88a6c45249057 `tools/README.md` and `tools/disasm/loader.py`.
These excerpts are historical context, not the current override workflow.

## Regeneration

If you regenerate the recompiled code (after fixing the recompiler or re-running with new options), you'll need to **re-apply manual patches** to the gen files. Keep a list of your patches — this is the most error-prone part of the workflow.

Recommended workflow:
1. Keep manual overrides in a separate file (`recomp_manual.c`)
2. Use `#if 0` / `#endif` to disable gen functions that have manual replacements
3. Track gen patches in a document (see CLAUDE.md in the burnout3 repo for an example)

## Historical loader argument description

```text
analysis_json: Optional path to burnout3_analysis.json.
```
