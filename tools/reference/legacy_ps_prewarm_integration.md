# Historical PS warmup integration commands

Source: XboxRecomp 6ee217d95a50e48182f5275f7f78f7ab00ed7ef9 `tests/nv2a_ps_prewarm/README.md`.
These commands assumed the game checkout and its environment script; they are
preserved as integration history rather than standalone toolkit instructions.

Build from the game repository after entering `scripts/enter-env.ps1`:

```powershell
cmake -S tools/xboxrecomp/tests/nv2a_ps_prewarm -B conformance_tmp/stutter-ps-prewarm-test -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build conformance_tmp/stutter-ps-prewarm-test
ctest --test-dir conformance_tmp/stutter-ps-prewarm-test --output-on-failure
```
