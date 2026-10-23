# Historical SSE stub classification assumptions

The prior recognizer described the following observed application behavior:

```text
- These form the game's data-driven parameter initialization system:
  thousands of tiny functions that copy/compute float values between
  .rdata constants and .data globals.
```

It restricted candidates to at most 120 bytes, described as the longest
observed chains. The generic recognizer now checks complete supplied function
bytes without this captured size ceiling. Its legacy data_init category and
confidence scores remain heuristic labels, not semantic or title proof.
