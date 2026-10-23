# Historical symbol-port observations

Docstrings from fork 119bf71. Historical donor/target results are not newly reproduced.

## rw_port.py

```python
"""Port RenderWare function names from a PE .exe donor (with matching MAP) onto
a target XBE by byte-matching the statically-linked RenderWare library code.

Unlike XDK libraries (own sections: D3D, DSOUND...), RenderWare is linked into
.text mixed with game code, so we match within .text and keep only names that
land on a detected function start in the target. Donor is the .exe because
Xyanide's XBE is a different build from its MAP; the .exe matches the MAP and
its .text bytes are what imagebld would copy into an XBE.

IMPORTANT -- the donor and target must use the SAME RenderWare version.
Microsoft's XDK libraries are byte-stable across versions, so map_names.py port
works across XDK versions; RenderWare is NOT. Tried against Burnout 3 (RW36,
release), Xyanide (an earlier RW) matched 1 of 908 -- the versions diverge too
far for a byte signature. Check the embedded "//RenderWare/RWxxActive/" tag in
both binaries and only port between matching xx. Burnout 3 is RW36; we have no
RW36 symbol-bearing donor, so this could not name it.
"""
```

## lib_port.py

```python
"""Name a target XBE's statically-linked library functions by byte-matching a
COFF .lib (which carries function name + code together -- no MAP needed).

Parses the archive's object members, pulls each function's opening bytes from
its .text, and searches for that signature in the target's .text, keeping
unique matches that land on a detected function start. Used here to test
whether RenderWare 3.7 (rwcore.lib) matches Burnout 3's RW36.
"""
```
