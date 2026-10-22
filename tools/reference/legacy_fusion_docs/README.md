# Historical Fusion corpus and adoption notes

Source: XboxRecomp babce61170c030315a00f04aee362f48cba8e902.

These original documents contain captured-title measurements, addresses,
analysis claims and past implementation plans. They are preserved as history,
not current compatibility, availability, correctness or completion evidence.
Their original text and relative references are retained; some historical
links refer to the original toolkit layout. Current reusable interfaces are
documented in the fork under `docs/technical/fusion-analysis.md`.

- `ms-fusion-recompiler.md`: source blob `36d05a83c2b4c67ca0ca586678069165c0fb7594`.
- `ms-fusion-codegen-teardown.md`: source blob `25e3cf8798b3fbf7b1af4621311301d6f3139c89`.
- `ms-fusion-corpus.md`: source blob `ebd6065d19ff1e042660d7e1a315fd9757d43346`.
- `ms-fusion-adoption-plan.md`: source blob `ae3cf097fb65f5a5fb08b8552d131b4acdd1fd90`.
- `svod-extraction.md`: source blob `55d07cf921e0866cdd9b28905dabb4753d7c229b`.

## Original feedback provenance descriptions

### src/kernel/icall_feedback.c

```text
/*
 * Indirect-branch target feedback.
 *
 * Records which guest addresses the title actually reaches through indirect
 * branches, so the next codegen run can seed function detection with them
 * instead of guessing. This is the local equivalent of the two inputs
 * Microsoft's own recompiler takes -- VirtualDispatchTraceFiles (recorded
 * indirect-branch target sets) and UpdateEnlightenments (a persisted analysis
 * database the compiler rewrites every build). See
 * docs/technical/ms-fusion-codegen-teardown.md.
 *
 * Static analysis cannot see where a vtable call goes. Running the title can.
 * tools/recomp/analyze_unresolved.py currently classifies unresolved targets by
 * where they land relative to known functions, which is inference; this is
 * measurement.
 *
 * Opt-in: define RECOMP_ICALL_FEEDBACK. Without it this file compiles to
 * nothing and the RECOMP_ICALL_OBSERVE hook in recomp_types.h expands to
 * (void)0, so default builds pay neither the store nor an observation array.
 *
 * Cost when enabled: one byte OR per indirect branch into the configured
 * executable window. Observations are best-effort under concurrent writers;
 * repeated runs merge their flags. The same hot target occupies one byte.
 */
```

### src/kernel/recomp_icall_feedback.h

```text
/*
 * Indirect-branch target feedback - shared constants.
 *
 * Included by both src/kernel/icall_feedback.c (which defines the array) and
 * templates/runtime/recomp_types.h (which emits the store into the ICALL
 * macros). Kept separate from recomp_types.h so the generated-code header stays
 * a template and this stays a runtime detail.
 *
 * See src/kernel/icall_feedback.c for the rationale and
 * docs/technical/ms-fusion-codegen-teardown.md for where the idea comes from.
 */
```

### tools/recomp/icall_feedback.py

```text

Indirect-branch target feedback: merge runtime observations into a persisted
database, and feed them back as function-detection seeds.

Static analysis cannot see where a vtable call goes. Running the title can.
This closes that loop, the same way Microsoft's own recompiler does with
VirtualDispatchTraceFiles (recorded indirect-branch target sets) and
UpdateEnlightenments (a persisted analysis database the compiler rewrites on
every build). See docs/technical/ms-fusion-codegen-teardown.md.

Workflow:

  1. Build the title with RECOMP_ICALL_FEEDBACK defined, run it, and call
     recomp_icall_feedback_dump("icall.txt") from atexit and your crash handler.
  2. python -m tools.recomp.icall_feedback merge icall.txt
  3. python -m tools.disasm --seed-functions tools/recomp/output/icall_targets.json
  4. Re-run the recompiler. Targets that were unresolved stubs are now real
     functions. Repeat -- each pass reaches further into the title, so it
     converges rather than being one-shot.

The same run also records, per `call` site, which targets that site reached
(written beside the dump as icall_sites.dump, or <dump>.sites). `merge`
unions those into tools/recomp/output/icall_sites.json, and tools.recomp reads
that file (--icall-sites): a site whose whole recorded set is at most four
translated functions is lifted as guarded direct calls, with the generic
dispatch kept as the fallback. An unseen target costs a lookup, never a wrong
call. A site that reached more targets than the runtime records is marked
saturated and never guarded.

The database is *cumulative*. A target observed in an earlier run is never
dropped because a later run did not reach it; that is the whole point of
persisting it, and it is why step 4 converges. Delete the file to start over.

The database is written in the format tools/disasm --seed-functions already
accepts (a list of {"start": "0x..."}), so it needs no conversion step. Extra
keys are ignored by that loader.

```
