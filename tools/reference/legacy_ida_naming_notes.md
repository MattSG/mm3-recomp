# Historical IDA naming assumptions

The previous exporter required the reference image base 0x00010000. The
current exporter takes optional caller-supplied --image-base validation.
Database addresses must still match the source XBE; export does not prove it.

## Prior exporter introduction and import advice

```text
xboxrecomp already had a Ghidra path and no IDA one. The gap was never the
analysis -- IDA's FLIRT and demangler recover the same kind of names Ghidra's
FidDb and RTTI analyzers do -- it was that nothing wrote them out in the shape
``tools/ghidra_naming/merge_names.py`` consumes. So this is that, and nothing
else: the merge, the placeholder filter, the C-identifier sanitising, the
collision handling and ``--apply`` are all the existing tool's job.

Addresses are absolute Xbox VAs, which is what ``functions.json`` keys on. That
only holds if the database is based where the XBE is: load the flat image from
``tools/ghidra_naming/extract_for_ghidra.py`` (it is a plain binary, nothing in
it is Ghidra-specific) as x86 32-bit at 0x00010000, or use a loader that does
the same. The script refuses to guess and says so if the base looks wrong.

```

## Captured guide statistics

The prior guide cited 735 names from two donor MAPs versus 504 from one,
and a 7.6 MB image. These are historical observations, not generic coverage
or performance guarantees.
