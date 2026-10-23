# MM3 function-identification keyword policy

`tools/mm3_func_id_keywords.json` preserves the nine inherited application
keyword categories formerly implicit in the shared function identifier.
They are heuristic hints, not verified function identities.

From `tools/xboxrecomp`, opt in explicitly:

```powershell
python -m tools.func_id path/to/default.xbe --game-keywords ../../tools/mm3_func_id_keywords.json
```

Pass the usual --functions, --strings, --xrefs and --output paths for the input.
Other consumers get no application keyword policy unless they supply one.
The MM3 list matches the former dictionary exactly, including ordering.
