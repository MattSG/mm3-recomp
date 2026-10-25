# Contributing

Fixes, reviews, documentation improvements, and small cleanups are welcome.
For substantial features or architectural changes, open an issue first to
discuss the approach. Search existing issues and PRs before starting.

## Submitting a pull request

1. Fork the repository and create a branch from `main`.
2. Follow the [README setup instructions](README.md#build-and-play) using your
   own lawfully obtained game dump. Keep generated game code and assets local.
3. Keep the PR focused on one problem; avoid unrelated formatting or rewrites.
4. Build and test the affected behaviour locally. For gameplay changes, describe
   what you actually verified in game; a successful build alone is insufficient.
5. Open a PR against `main` explaining the problem, the change, reproduction
   steps, testing performed, and any limitations or untested behaviour. Link the
   relevant issue if there is one. Draft PRs are welcome for unfinished work.

For performance changes, include comparable before/after measurements with the
same scene, settings, and hardware. Documentation-only changes need no game build.
GitHub Actions is disabled; include local validation results in your PR.

AI-assisted contributions are welcome. Review and understand the code you submit,
check its provenance, and describe your testing. Do not claim tests you did not run.

## Reporting bugs

Include the commit, Windows version, CPU/GPU, relevant settings, steps to reproduce,
expected and actual behaviour, and relevant logs. Remove personal data and secrets
from logs before posting. State whether the problem is reproducible.

## What you may submit

Follow the [copyright and upload restrictions](README.md#copyright-and-contributions).
Do not upload game code, generated translations, game assets, disc images, DLC,
proprietary SDK files, firmware, keys, private saves, or unauthorised download links.
These restrictions also apply to issue and PR attachments and build artifacts.

Submit only material you created or have permission to contribute under the
applicable licence. Original project contributions use
[GPL-3.0-only](LICENSE); third-party components retain their own licences and
notices. Identify any third-party code you introduce and its source and licence.

The runtime in `tools/xboxrecomp` is upstream
[sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp), pinned to a
release. It retains its own licence and
[contribution guide](tools/xboxrecomp/CONTRIBUTING.md). The generic changes this
port still needs are the patch series in `patches/xboxrecomp`, applied to the
submodule's working tree by `scripts/patch_xboxrecomp.py` (setup and CMake run
it). They include AI-written code; review and test them with the same care.

To change the runtime, commit in `tools/xboxrecomp` on top of the pin and
regenerate the series (the script's header has the command). Patches stay
game-agnostic so each can be offered upstream; once one is merged there, move
the pin and drop it.

Car Workshop defaults are tracked in `tools/car-tools/car-mods/cars.json`; personal model
selections and tuning belong in the ignored `cars.local.json`. Game assets,
prepared model packages and extracted import sources remain local. Tests using
optional fixtures or an independent decoder must report missing coverage as
skipped, not as verified game or imported-model behavior.
