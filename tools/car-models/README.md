# Imported car visuals

These tools turn downloaded MM1/MM2 car resources into inspectable geometry,
then build prepared MM3 visual packages for Car Workshop. The first prepared
port is **Panoz GTR-1**, replacing the exterior of the existing Panoz slot
(`9004`, Viper donor). The native showroom, Cruise loading and on-road driving
have been verified. Live telemetry recorded acceleration from 6 to 78 km/h.
Reapplying the Workshop pack changed zero files and preserved the model.
Screenshots and telemetry are saved in `out/car-models/verification`.

The expanded import roster adds the eight sources listed below. **All eight
additional imports passed native showroom checks**, and the live catalog
contains all **49 car IDs** (35 original plus 14 extra slots). The rebuilt
runtime includes manufacturer, model and short names for all fourteen added
cars; its 42-label native contract passed. The Senna GTR header was also
confirmed in the current binary. Screenshots are collected in
`out/car-models/verification/remaining/index.html`.

Car Workshop supports all fourteen extra-car slots;
the original Mini/Mustang/Viper Race and Bugatti variants retain their own
settings and donor visuals unless given a package.

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

Run the commands below from the repository root. These model tools require
**NumPy and Pillow**, separately from Workshop's standard-library GUI. The
system Python on this computer has both; the Workshop's smaller `tools/venv`
environment does not include Pillow. Check your selected interpreter with:

```powershell
python -c "import numpy, PIL; print('Model tools ready')"
```

On a fresh machine, install missing model-tool dependencies into your selected
Python environment with `python -m pip install numpy Pillow`. The normal
game-build setup installs the recompilation dependency, not these optional
conversion tools. Downloaded car sources, exported models, generated packages
and verification images remain local under ignored folders. Workshop prefers
the ignored `tools/car-mods/cars.local.json` when it exists; the tracked
`cars.json` contains donor-compatible defaults without local model paths.

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
    --car-id 9004 --icons out/car-models/previews/panoz
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

The source's high-detail exterior is repeated across the donor's internal LODs
so the old donor body does not return at distance. Large meshes are split into
sibling chunks beneath the same native transform, preserving the animation of
each wheel assembly. Dedicated low-detail imported meshes can be added later.
The retained native car shader supports at most 23 material
slots; sources exceeding that limit need materials merged before packaging.

## 4. Select the package in Car Workshop

Close MM3, then open Workshop directly on the Panoz Model tab:

```powershell
tools\car-mods\editor.cmd --car-id 9004 --tab model
```

Choose the prepared `out/car-models/packages/Panoz` folder, which contains
`package.json`, then click **Save & Apply to game**. Restart MM3 and select
**Panoz GTR-1**. The selection is stored as the car's `model_package` path in
`tools/car-mods/cars.json`; relative paths resolve beside that manifest.
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
tools\car-mods\editor.cmd --car-id 9010 --tab model
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
current eight packages have native showroom evidence, with Cruise driving
evidence for Senna GTR and Ferrari F2007. Separate donor collision, dashboard
and effects remain in use; fixed initial liveries and opaque windows are still
the limits of this first visual port.
Some imported Koenigsegg-donor packages, including Senna and Regera, show jagged
ground shadows. This predates the runtime optimizations and remains unresolved.

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
