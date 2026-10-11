# Imported car visuals

The current converter generates corrected closed stencil shadows for all nine
imported exteriors. For a fresh checkout, use the complete source recipe and
optional imported preset below. It carries the same ten-car tuning as the
default preset, including Mini Rally's native visuals.

## Extra car names and model credits

The [imported preset](../car-mods/cars.imported.json) has ten extra slots:
nine imported exteriors from eight downloadable packs, plus Mini Rally's
original MM3 exterior. The shorter Workshop names are shown alongside the
exact source-model names below. Source credits were checked against the
original MM2X listings and the readmes included with the downloaded packs.

| Workshop name (car ID) | Exterior model | Original model pack / source | Model and conversion credits |
|---|---|---|---|
| Mini Rally (9001) | Original MM3 Mini | Your own Midtown Madness 3 game data; no external model download | Original MM3 assets by the game's development team, Digital Illusions CE (DICE), published by Microsoft Game Studios. No individual Mini model artist is identified here. |
| Panoz GTR-1 (9004) | Panoz GTR-1 | [Panoz GTR-1](https://www.mm2x.com/download-1336-panoz-gtr-1#1336), including its paintjob patch | ISI: original rFactor model; Aaro4130: MM2 conversion; Thunder-ice and Alexi99: skins. |
| McLaren F1 (9005) | 1995 McLaren F1 GTR | [1995 McLaren F1 GTR](https://www.mm2x.com/download-2214-1995-mclaren-f1-gtr#2214) | Firemint: Real Racing 2 model and textures; Tsutarja495: MM2 conversion. |
| Formula 1 (9006) | Ferrari F2007 F1 | [Ferrari F2007 F1](https://www.mm2x.com/download-1220-ferrari-f2007-f1#1220) | Image Space Incorporated: original rFactor model; Kunal / Kunal2045: MM2 conversion. The readme records an intermediate GTA IV conversion; see the additional pack credits below. |
| Ford GT (9009) | 2017 Ford GT, low-detail model | [2017 Ford GT](https://www.mm2x.com/download-2049-2017-ford-gt#2049), an MM1 pack | Firemonkeys: Real Racing 3 model; Eclipse 72rus: model extraction; mm2mm1fan: MM1 conversion. |
| McLaren Senna (9010) | 2019 McLaren Senna | [McLaren Senna 2-Pack](https://www.mm2x.com/download-2221-mclaren-senna-2-pack#2221) | Firemonkeys: Real Racing 3 model and textures; Tsutarja495: MM2 conversion. |
| McLaren Senna GTR (9011) | 2020 McLaren Senna GTR | [McLaren Senna 2-Pack](https://www.mm2x.com/download-2221-mclaren-senna-2-pack#2221), the same pack as the road car | Firemonkeys: Real Racing 3 model and textures; Tsutarja495: MM2 conversion. |
| Ferrari FXX K (9012) | 2015 Ferrari FXX K | [2015 Ferrari FXX K](https://www.mm2x.com/download-1806-2015-ferrari-fxx-k#1806) | Firemonkeys Studios: Real Racing 3 source model; Matyii, also named Matyasd at MMArchive: MM2 conversion. |
| Koenigsegg Regera (9013) | 2015 Koenigsegg Regera | [2015 Koenigsegg Regera](https://www.mm2x.com/download-1715-2015-koenigsegg-regera#1715) | Firemonkeys Studios: Real Racing 3 source model; Matyii / Matyasd: MM2 conversion. |
| Lamborghini Aventador (9014) | Lamborghini Aventador LP700-4 | [Lamborghini Aventador LP700-4](https://www.mm2x.com/download-1465-lamborghini-aventador-lp700-4#1465) | Firemonkeys: Real Racing 3 body; Gameloft: GT Racing 2 wheels, brake discs and calipers; BUGATTIMAN253MPH: MM2 conversion. |

Microsoft's [original MM3 announcement](https://news.microsoft.com/source/2002/04/24/microsoft-game-studios-turns-midtown-madness-3-loose-on-city-streets/)
identifies the retail game's developer and publisher. Mini Rally reuses the
native `mini` assets under an extra-car alias. The other nine rows identify the
downloaded exterior models; the [roster below](#imported-roster) records their
MM3 collision, interior and sound donors. Workshop's manufacturer specification
references are separate sources for the driving presets. A source-pack year
or racing variant can therefore differ from the specification reference.

Additional pack credits are retained in the original readmes. The F2007
listing also names **BryanLex** without specifying a role, and credits **Riva**
for its MM2 dashboard; its readme thanks **Franch88**, **Riva** and **Sajmon14**
for testing and fixes. The Panoz readme credits **Angel Studios and Microsoft**
for its MM2 dashboard/showcase. Those pack features are not imported custom
MM3 interiors. Source-pack sounds likewise remain credited in their readmes;
the current MM3 packages use their native donor sounds.

The repository's MM3 conversion tools and generated shadow proxies build on
these authors' meshes and textures. Keep the original readmes and source terms
with a prepared package. The [reproduction recipe](#reproduce-the-nine-imported-exteriors)
lists the exact input archives and SHA-256 values; downloaded models, textures
and game assets are not included in this repository.

## Historical import verification

The observations in this section describe the initial imports, before the
current factory engine profiles and later shadow repairs. They are retained
as investigation history, not measurements of the current tuning.

These tools turn downloaded MM1/MM2 car resources into inspectable geometry,
then build prepared MM3 visual packages for Car Workshop. The first prepared
port is **Panoz GTR-1**, replacing the exterior of the existing Panoz slot
(`9004`, Viper donor). The native showroom, Cruise loading and on-road driving
have been verified. Live telemetry recorded acceleration from 6 to 78 km/h.
Reapplying the Workshop pack changed zero files and preserved the model.
Screenshots and telemetry are saved in `out/car-models/verification`.

The expanded import roster adds the eight sources listed below. **All eight
additional imports passed native showroom checks**. At that verification the
catalog contained 49 car IDs (35 original plus 14 extra slots), and its
42-label native contract passed. The current catalog contains **45 car IDs**:
35 originals and 10 extras, comprising these nine imported cars and Mini Rally.
The Mustang GT, Viper Race, Chiron and Bolide extra slots have since been
retired. The Senna GTR header was also confirmed during the original checks.
Screenshots are collected in
`out/car-models/verification/remaining/index.html`.

Car Workshop supports the ten active extra-car slots. Mini Rally retains its
native Mini body and shadow geometry in both near and far models; only their
asset paths are aliased. The nine imported cars receive generated body and
wheel shadow proxies when rebuilt. Retired IDs remain unused.

Cruise telemetry recorded the Senna GTR accelerating from **17.2 to 114.8
km/h** and the Ferrari F2007 from **8.0 to 90.5 km/h**. Runtime stat comparisons
matched their configuration except wheel radius, which MM3 derives from the
imported geometry: approximately **0.3083741 m** for Senna GTR and
**0.3922908 m** for F2007, versus the configured 0.34 m. The Python checks ran
66 cases: 65 passed and one optional Windows symbolic-link case was skipped.

After the second Cruise drive, a return to the menu soft-stalled. A read-only
snapshot placed the guest in frontend string-key lookup, and the inspected
log showed no allocator exhaustion or out-of-memory report. Its cause remains
uncertain; these visual checks do not establish that every menu transition or
crash is fixed. The observations are recorded in
`out/car-models/verification/remaining/menu-stall-audit.json`.

The initial package keeps the silver **#44 Visteon** livery. Its three MM3
material sets use the same imported paint, so Workshop paint controls do not
recolor this fixed texture. Stats, gearing and speedometer settings still work.
The exterior body and wheels come from the imported car. The driving setup,
collision, sounds and dashboard/cockpit remain based on the Viper donor.
Custom crash deformation, detachable parts, sound conversion and a custom
dashboard are outside this first visual port.

Imported alpha textures are baked over a dark base for this initial package.
This preserves the window tint, windshield banner and headlight details using
MM3's opaque material pass. The windows are opaque. The donor's low dashboard
mesh is hidden from the exterior because it protrudes through the Panoz roof;
the separate Viper dashboard asset remains available for the interior camera.

## Conversion requirements

Run the commands below from the repository root. These model tools require
**NumPy, SciPy and Pillow**, separately from Workshop's standard-library GUI. The
system Python on this computer has all three; the Workshop's smaller `tools/venv`
environment does not include Pillow. Check your selected interpreter with:

```powershell
python -c "import numpy, scipy, PIL; print('Model tools ready')"
```

On a fresh machine, install missing model-tool dependencies into your selected
Python environment with `python -m pip install numpy scipy Pillow`. SciPy builds
the closed convex shadow proxies; it is a conversion dependency, not a game
runtime dependency. The normal
game-build setup installs the recompilation dependency, not these optional
conversion tools. Downloaded car sources, exported models, generated packages
and verification images remain local under ignored folders. Workshop prefers
the ignored `tools/car-mods/cars.local.json` when it exists; the tracked
`cars.json` contains donor-compatible defaults without local model paths.

## Reproduce the nine imported exteriors

Use the same source revision as your game build, such as the `v2026.10.12` tag,
and first complete the [repository build and game-data setup](../../README.md).
The converter reads the original Viper, Koenigsegg and Lotus models from your
extracted `game_files/Data`; a compiled executable alone is insufficient.
Neither the game data nor downloaded car meshes/textures are distributed here.

Download these eight packs from their original MM2X listings. Download forms
may require a browser and a security challenge. Retain each original readme
and its terms; these links and conversion tools grant no additional rights to
redistribute the source models, textures or derived packages.

Under `out/car-models/downloads`, create the input folders shown below and put
the listed `.ar` file and the original readme directly inside each folder.
Panoz also requires its included `!!GTR-1_Paintjobs.ar` overlay. The Senna pack
supplies both Senna models from one archive. These names are independent of
the outer ZIP filename or the folder name chosen by the download website.

| Source listing | Input folder / archive | Export basename | Source folder / package alias | Car ID |
|---|---|---|---|---|
| [Panoz GTR-1](https://www.mm2x.com/download-1336-panoz-gtr-1#1336) | `panoz/!GTR-1.ar` + `panoz/!!GTR-1_Paintjobs.ar` | `pgtr1` | `panoz` / `Panoz` | 9004 |
| [1995 McLaren F1 GTR](https://www.mm2x.com/download-2214-1995-mclaren-f1-gtr#2214) | `mclaren-f1/!mcl_f1gtr.ar` | `mcl_f1gtr` | `mclaren-f1` / `McLaren_F1` | 9005 |
| [Ferrari F2007 F1](https://www.mm2x.com/download-1220-ferrari-f2007-f1#1220) | `ferrari-f2007/!F2007.ar` | `f2007` | `ferrari-f2007` / `Form1` | 9006 |
| [2017 Ford GT](https://www.mm2x.com/download-2049-2017-ford-gt#2049) | `ford-gt/vpfgt17.ar` | `vpfgt17` | `ford-gt` / `Ford_GTcar` | 9009 |
| [McLaren Senna 2-Pack](https://www.mm2x.com/download-2221-mclaren-senna-2-pack#2221) | `senna/!mcl_senna.ar` | `mcl_senna` | `senna` / `Senna_Road` | 9010 |
| [McLaren Senna 2-Pack](https://www.mm2x.com/download-2221-mclaren-senna-2-pack#2221) | `senna/!mcl_senna.ar` | `mcl_sennagtr` | `senna-gtr` / `Senna_GTR1` | 9011 |
| [2015 Ferrari FXX K](https://www.mm2x.com/download-1806-2015-ferrari-fxx-k#1806) | `fxx-k/!FXX_K.ar` | `fxxk` | `fxx-k` / `FXXK_Racer` | 9012 |
| [2015 Koenigsegg Regera](https://www.mm2x.com/download-1715-2015-koenigsegg-regera#1715) | `regera/!Regera.ar` | `regera` | `regera` / `Regera_GT1` | 9013 |
| [Lamborghini Aventador LP700-4](https://www.mm2x.com/download-1465-lamborghini-aventador-lp700-4#1465) | `aventador/!lamboaven.ar` | `lamboaven` | `aventador` / `AventadorX` | 9014 |

These SHA-256 values identify the actual input archives used for this roster,
after unpacking the downloads. They are provenance checks, not a license.
Use `Get-FileHash -Algorithm SHA256 <archive-path>` to compare an input.

| Archive | SHA-256 |
|---|---|
| `!GTR-1.ar` | `f547db98b55cd99815d8a5506a543c943addd6c82b74f91986087fbc55a9c072` |
| `!!GTR-1_Paintjobs.ar` | `9680a6907f95a694a1149f25ed02bfbad1ce496c2db7440c0c4574f05074adb4` |
| `!mcl_f1gtr.ar` | `6db5d57a770aa0b46fa71b8e65e20c8bd0e3e180248bb2aaa233809f521ea82a` |
| `!F2007.ar` | `c1f77d7e4988e88262158371cb9a053ce0971cd6c0d68b1d80e91e604cad346c` |
| `vpfgt17.ar` | `ccb468d2eaf0bcf58563cc109c14b528fcfd1257a131aaafd7330d65377cb603` |
| `!mcl_senna.ar` | `3669ba132ed524a567fb331742614297e57d7f0972b7d1ae37df0ad112ee824d` |
| `!FXX_K.ar` | `c19aa6bb2b125e6007e288c85faa9433a83a574ae2eb545911dc2418c1819315` |
| `!Regera.ar` | `808d1ae149cccbaf8e2597a1c273fee60192baceb1861fb71a9aafd0c107e8f0` |
| `!lamboaven.ar` | `3dfe8f0a4230660ea25cf772c8eb5612ee79cf4319ddc6f0c7bf738c13bae764` |

Run this PowerShell recipe from the repository root with a Python interpreter
containing NumPy, SciPy and Pillow. Every export uses paint job **0** and source
LOD **H**. The preview and package include the complete static `BODY`, `BREAK`
and `FNDR` exterior plus four separately animated wheels. Panoz uses the
recorded **-0.36 m** vertical offset; the other eight use automatic wheel-pivot
alignment. Only F2007 enables averaged minified repeats. Texture size is
explicitly capped at 1024 pixels. Existing output folders are regenerated.

```powershell
$ErrorActionPreference = 'Stop'
$downloads = 'out/car-models/downloads'
$recipes = @(
    @{Id=9004; Source='panoz'; Alias='Panoz'; Archive='panoz/!GTR-1.ar'; Model='pgtr1'},
    @{Id=9005; Source='mclaren-f1'; Alias='McLaren_F1'; Archive='mclaren-f1/!mcl_f1gtr.ar'; Model='mcl_f1gtr'},
    @{Id=9006; Source='ferrari-f2007'; Alias='Form1'; Archive='ferrari-f2007/!F2007.ar'; Model='f2007'},
    @{Id=9009; Source='ford-gt'; Alias='Ford_GTcar'; Archive='ford-gt/vpfgt17.ar'; Model='vpfgt17'},
    @{Id=9010; Source='senna'; Alias='Senna_Road'; Archive='senna/!mcl_senna.ar'; Model='mcl_senna'},
    @{Id=9011; Source='senna-gtr'; Alias='Senna_GTR1'; Archive='senna/!mcl_senna.ar'; Model='mcl_sennagtr'},
    @{Id=9012; Source='fxx-k'; Alias='FXXK_Racer'; Archive='fxx-k/!FXX_K.ar'; Model='fxxk'},
    @{Id=9013; Source='regera'; Alias='Regera_GT1'; Archive='regera/!Regera.ar'; Model='regera'},
    @{Id=9014; Source='aventador'; Alias='AventadorX'; Archive='aventador/!lamboaven.ar'; Model='lamboaven'}
)
foreach ($recipe in $recipes) {
    $archive = Join-Path $downloads $recipe.Archive
    $source = "out/car-models/source/$($recipe.Source)"
    $preview = "out/car-models/previews/$($recipe.Source)"
    $package = "out/car-models/packages/$($recipe.Alias)"
    $importArgs = @('tools/car-models/mm2_source.py', 'import', $archive,
        '--model', $recipe.Model, '--paintjob', '0', '--lod', 'H', '--out', $source)
    if ($recipe.Id -eq 9004) {
        $importArgs += @('--overlay', (Join-Path $downloads 'panoz/!!GTR-1_Paintjobs.ar'))
    }
    python @importArgs
    if ($LASTEXITCODE -ne 0) { throw "Import failed: $($recipe.Alias)" }
    Get-ChildItem -LiteralPath (Split-Path -Parent $archive) -Filter '*readme*.txt' -File |
        Copy-Item -Destination $source
    python tools/car-models/preview.py "$source/model.json" --out $preview `
        --name $recipe.Alias --include '(?:BODY|BREAK\d+|FNDR[01]|WHL[0-3])_H' `
        --icons --icon-prefix Car
    if ($LASTEXITCODE -ne 0) { throw "Preview failed: $($recipe.Alias)" }
    Copy-Item -LiteralPath (Join-Path $preview 'Car_Menu.png') -Destination (Join-Path $preview 'Menu.png')
    Copy-Item -LiteralPath (Join-Path $preview 'Car_Small.png') -Destination (Join-Path $preview 'Small.png')
    $buildArgs = @('tools/car-models/build_package.py', "$source/model.json", $package,
        '--car-id', $recipe.Id, '--icons', $preview, '--max-texture-size', '1024')
    if ($recipe.Id -eq 9004) { $buildArgs += @('--y-offset', '-0.36') }
    if ($recipe.Id -eq 9006) { $buildArgs += '--average-minified-repeats' }
    python @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Package failed: $($recipe.Alias)" }
}
```

The resulting nine folders match the relative paths in
[`cars.imported.json`](../car-mods/cars.imported.json). This optional tracked
preset is identical to `cars.json` in tuning, paint, ratings, factory profiles
and speedometer ranges; only the nine `model_package` selections differ.
Mini Rally keeps its native Mini model and requires no download. Close the
game, preview the installation, then apply this explicit preset:

```powershell
python tools/car-mods/prepare.py --config tools/car-mods/cars.imported.json
python tools/car-mods/prepare.py --config tools/car-mods/cars.imported.json --apply
tools\car-mods\editor.cmd --config tools/car-mods/cars.imported.json --car-id 9004 --tab model
```

The explicit `--config` avoids an existing `cars.local.json` silently selecting
a different setup. To use the same ten-car tuning with donor visuals instead,
apply `tools/car-mods/cars.json` explicitly. For personal edits without modifying
the tracked imported preset, copy it to `cars.local.json` and then select that
file explicitly. Rebuild and reapply both near/far models to receive shadow
writer updates; recompiling the game does not rewrite prepared packages.

This reproduces the tagged code, tuning, chosen source liveries and current
conversion behavior, including closed body/wheel shadow proxies. It does not
promise byte-identical historical packages: the development installation had
its existing visible geometry preserved while shadows were repaired, whereas
this recipe converts the original sources afresh. In particular, Panoz's older
visible packing can differ. Generated reports also contain local source paths.
Keep previews and verify the result in-game; fixed liveries, opaque glass,
convex shadow approximations and donor collision/interiors remain the limits.

## 1. Import the source car

Unpack the downloaded car archive and retain its readme and any paint patch.
Select the actual `.ar` resource archive, rather than a ZIP containing several
car archives. The importer reads MM2 ZIP/AR/PKG resources and MM1 ARES/BMS
resources without executing their contents or modifying MM3's game files.

For the Panoz, the internal model basename is `pgtr1`:

```powershell
$panozArchive = 'C:\path\to\pgtr1.ar'
python tools/car-models/mm2_source.py import $panozArchive `
    --model pgtr1 --paintjob 0 --lod H `
    --out out/car-models/source/panoz
```

Add `--overlay 'C:\path\to\paint-patch.ar'` when the download includes a
separate resource patch. Overlays are applied after the main archive. The
Panoz source inspected here includes 19 paint jobs; `--paintjob` selects one
for export. Choosing another source paint job requires rebuilding the package.

The output contains `model.json` and `textures/*.png`. It preserves mesh
transforms, wheel positions, material assignments and authored UVs. Coordinates
use meters, **+Y up** and **front toward -Z**, with top-left texture UVs. Keep
this directory available for rebuilding. Copy the original readme into this
source directory too, so the package builder can retain it.

For archive inspection, these commands write separate reports/resources:

```powershell
python tools/car-models/mm2_source.py inventory 'C:\path\to\unpacked-cars' `
    --out out/car-models/source-inventory.json
python tools/car-models/mm2_source.py extract $panozArchive `
    --out out/car-models/reference/panoz
```

## 2. Check the source visuals

Render the actual textured body and wheels before modifying MM3:

```powershell
python tools/car-models/preview.py out/car-models/source/panoz/model.json `
    --out out/car-models/previews/panoz --name 'Panoz GTR-1' `
    --icons --icon-prefix Panoz
Copy-Item out/car-models/previews/panoz/Panoz_Menu.png out/car-models/previews/panoz/Menu.png
Copy-Item out/car-models/previews/panoz/Panoz_Small.png out/car-models/previews/panoz/Small.png
```

Open `out/car-models/previews/panoz/index.html` to inspect front, rear and side
views. `source-check.json` records dimensions, wheel origins, material usage,
UV ranges and winding checks. The Panoz measures approximately **1.983 m wide
and 4.471 m long**; its height includes the roof antenna. The renderer draws
both sides of triangles, so these images alone do not prove native culling is
correct. Wheel positions, readable logos and UV orientation should also be
checked in-game after installation.

The Panoz uses repeating UVs, including negative wheel V coordinates. The
preview preserves that tiling. The MM3 writer translates UVs by whole texture
periods and clips broad tiled triangles where needed; clamping the source UVs
would distort its textures.

For textures whose RGBA pixels are all identical, the builder can replace
their UVs with one sample coordinate exactly: the rendered color is unchanged.
It duplicates affected vertices when necessary so neighboring detailed faces
keep their original UVs. The default conversion retains full UV detail and
rejects excessively broad repeats instead of silently flattening them.

`--icons` also produces transparent `Panoz_Menu.png` (**128 x 64**) and
`Panoz_Small.png` (**64 x 64**), matching the donor menu image dimensions.
Other sources can use `--include` with a regular expression selecting their
body/wheel mesh names. For the complete static exterior of the other imported
MM2 cars, use `--include '(?:BODY|BREAK.*|FNDR.*|WHL[0-3])_H'`: wings, mirrors
and fenders can live outside `BODY_H`. These parts are included as static
visuals for this first port. The default selects `BODY_H` and `WHL0_H` through
`WHL3_H`. Copy the rendered icons to the generic `Menu.png` and `Small.png`
names expected by the package builder, as above.

## 3. Build a prepared MM3 package

```powershell
python tools/car-models/build_package.py `
    out/car-models/source/panoz/model.json out/car-models/packages/Panoz `
    --car-id 9004 --y-offset -0.36 --icons out/car-models/previews/panoz
```

This creates a package folder; it does not install it. The builder supports
**Viper, Koenigsegg and Lotus donors** with the supported body/wheel naming
convention. It includes `BODY`, `BREAK` and `FNDR` exterior parts and separately
animated wheels. Other donor material maps and differently structured models
need adaptation and validation first. By default, vertical alignment compares
the **average authored source wheel-pivot height** with the **average native
donor wheel-pivot height**. It translates the source by that difference;
`--y-offset` can override the automatic placement when an in-game check shows
it is needed.
Textures default to a maximum size of 1024 pixels, adjustable with
`--max-texture-size`.

The package contains:

- `package.json`, identifying the target car and its owned asset paths.
- Near/far `.cmp` models with the imported exterior and wheels.
- `.omb` material sets and MM3 `.cdds` textures.
- Imported menu thumbnails when `--icons` is supplied.
- `conversion-report.json`, recording material mapping, mesh replacements and
  alignment settings, plus source readmes copied from the source directory.

The imported exterior replaces both the near model and the separate `_lod1`
distance model. The second mesh slot is a dedicated stencil shadow pass. Each
body and wheel gets a simplified convex proxy built from its own imported
positions, with native attributes and material 0. Facet caps and collapsed
edge quads form a closed volume when the native shader extrudes it. Wheel
shadows use the same animated parent as their visible geometry. Convex proxies
approximate concave openings and recesses; they are not exact body surfaces.
At most 482 support vertices give 960 caps and 2,880 side-wall triangles per
proxy, and the six coordinate extrema are preserved. Large visible meshes are
split into sibling chunks beneath the same native transform; the first sibling
casts the complete component shadow, and other siblings have empty shadow
placeholders. Dedicated low-detail visible meshes can be added later.
The retained native car shader supports at most 23 material
slots; sources exceeding that limit need materials merged before packaging.

## 4. Select the package in Car Workshop

Close MM3, then open Workshop directly on the Panoz Model tab:

```powershell
tools\car-mods\editor.cmd --config tools/car-mods/cars.imported.json --car-id 9004 --tab model
```

Choose the prepared `out/car-models/packages/Panoz` folder, which contains
`package.json`, then click **Save & Apply to game**. Restart MM3 and select
**Panoz GTR-1**. The selection is stored as the car's `model_package` path in
the selected manifest; relative paths resolve beside that manifest. The command
above explicitly selects `cars.imported.json`, regardless of local defaults.
Keep the prepared package folder available. Later stat or speedometer changes
reapply its visuals after donor cloning, preserving the imported body.

**Use donor model** clears the selection. Save and apply again to return to the
donor body. The installer validates the package target and confines its assets
to that car's visual folders, associated thumbnails and loading screen. It
uses the existing backup, integrity and rollback process and does not edit
player saves. Full restoration is documented in
[the Car Workshop README](../car-mods/README.md).

## Imported roster

The original Panoz import and eight additional sources use these existing or
new Workshop slots. Source folders are under `out/car-models/source`; package
folders are named for the fixed internal alias under
`out/car-models/packages`. Workshop labels and physics settings are separate
from the imported exterior's source model name.

| Car ID | Workshop slot | Imported exterior | MM3 donor | Source folder / package alias |
|---|---|---|---|---|
| 9004 | Panoz GTR-1 | Panoz GTR-1 | Viper | `panoz` / `Panoz` |
| 9005 | McLaren F1 | 1995 McLaren F1 GTR | Koenigsegg | `mclaren-f1` / `McLaren_F1` |
| 9006 | Formula 1 | Ferrari F2007 | Lotus | `ferrari-f2007` / `Form1` |
| 9009 | Ford GT | 2017 Ford GT, low-detail MM1 conversion | Koenigsegg | `ford-gt` / `Ford_GTcar` |
| 9010 | McLaren Senna | McLaren Senna | Koenigsegg | `senna` / `Senna_Road` |
| 9011 | McLaren Senna GTR | McLaren Senna GTR | Koenigsegg | `senna-gtr` / `Senna_GTR1` |
| 9012 | Ferrari FXX K | Ferrari FXX K | Koenigsegg | `fxx-k` / `FXXK_Racer` |
| 9013 | Koenigsegg Regera | Koenigsegg Regera | Koenigsegg | `regera` / `Regera_GT1` |
| 9014 | Lamborghini Aventador | Lamborghini Aventador LP700-4 | Koenigsegg | `aventador` / `AventadorX` |

For example, prepare the Senna's already exported neutral source:

```powershell
python tools/car-models/preview.py out/car-models/source/senna/model.json `
    --out out/car-models/previews/senna --name 'McLaren Senna' `
    --include '(?:BODY|BREAK.*|FNDR.*|WHL[0-3])_H' --icons --icon-prefix Senna
Copy-Item out/car-models/previews/senna/Senna_Menu.png out/car-models/previews/senna/Menu.png
Copy-Item out/car-models/previews/senna/Senna_Small.png out/car-models/previews/senna/Small.png
python tools/car-models/build_package.py `
    out/car-models/source/senna/model.json out/car-models/packages/Senna_Road `
    --car-id 9010 --icons out/car-models/previews/senna
tools\car-mods\editor.cmd --config tools/car-mods/cars.imported.json --car-id 9010 --tab model
```

Select `out/car-models/packages/Senna_Road` in the Model tab. The other cars
follow the same pattern using their source folder, car ID and alias from the
table. The Ford source uses MM1 BMS; the other eight use MM2 PKG3. All start
with one selected source livery. Additional factory colors and racing liveries
can be prepared as separate rebuilds later.

The Ford importer also handles the MM1 TGA row direction before exporting
top-left PNG textures; use the current source export rather than an older
version with inverted texture rows.

For this roster, **only Ferrari F2007** needs the opt-in
`--average-minified-repeats` conversion. Two carbon-textured source faces span
approximately 6,975 and 7,285 UV cells. Repeating and subdividing every cell
would produce excessive geometry, so this option substitutes the texture's
1 x 1 mip average for those two heavily minified faces. The resulting material
color is the original diffuse color multiplied by the texture average. Other
faces, their texture detail, and all source `model.json`/PNG files remain
unchanged. The affected faces are recorded in `conversion-report.json`.
Keep the flag off for the other imports.

```powershell
python tools/car-models/build_package.py `
    out/car-models/source/ferrari-f2007/model.json out/car-models/packages/Form1 `
    --car-id 9006 --average-minified-repeats `
    --icons out/car-models/previews/ferrari-f2007
```

A successful neutral export or prepared package alone does not establish MM3
shader compatibility, correct native winding, wheel animation or runtime
appearance. Keep the source previews as references when rebuilding. The
initial eight additional packages had native showroom evidence, with Cruise driving
evidence for Senna GTR and Ferrari F2007. Separate donor collision, dashboard
and effects remain in use; fixed initial liveries and opaque windows are still
the limits of this first visual port.
The exporter previously treated dedicated shadow slots as visible LODs and
wrote ordinary textured surfaces into them. The native shadow shader requires
closed stencil volumes with facet normals and collapsed edge walls; ordinary
surface geometry leaves gaps when extruded. Changing vertex attributes alone
does not repair that topology. Rebuilding now generates closed body and wheel
proxies. Existing packages need their near and `_lod1` models regenerated
and reapplied through Workshop to receive this correction.

## Preserve source credits

Keep the downloaded archive and its original readme alongside each source.
The Panoz readme credits **ISI** for the original rFactor model, **Aaro4130**
for the MM2 conversion, and **Thunder-ice and Alexi99** for its skins. Its
dashboard/showcase credits name **Angel Studios and Microsoft**. This Panoz
source is from rFactor, rather than Real Racing 3. The F1 GTR readme names
**Firemint/Real Racing 2**; the Senna readme names **Firemonkeys**.

The F2007 readme credits **ISI** and its MM2 converter **Kunal**. Ford's readme
credits **Firemonkeys**, **Eclipse 72rus** and **mm2mm1fan** for its model and
conversion chain. FXX K and Regera readmes identify Real Racing 3 and converter
**Matyii/Matyasd**. Aventador's readme credits **BUGATTIMAN253MPH** for the MM2
version, **Firemonkeys** for the body and **Gameloft** for wheel/brake assets.
Original sound credits remain in the preserved readmes, even though these MM3
packages retain their donor sounds.

The preserved source readmes are also available in
`out/car-models/source/evidence`. The package builder copies readme `.txt`
files from the selected source directory into the prepared package. Retain
these credits when sharing a conversion and check the source author's terms
before redistributing its model or textures.
