# Extra cars and editable stats

Car Workshop supports nine separately selectable extra-car identities.
The original 35 cars remain in the catalog. Each variant has its own name,
ratings and driving setup, with an original MM3 donor providing physics,
interior, sound and collision. A selected prepared model package supplies its
custom body, wheels, textures and livery; a blank package selection uses the
donor visuals. Defaults keep the donor's gearbox, RPM thresholds, engine curve,
turbo, grip, brakes and drag. Only `Mass` and native MM3 `TorqueScale` are tuned
for enjoyable arcade driving. The real car specifications remain available as
reference information.

| Added car | Original MM3 donor |
|---|---|
| Panoz GTR-1 | Dodge Viper |
| McLaren F1 | Koenigsegg |
| Formula 1 | Lotus |
| Ford GT | Koenigsegg |
| McLaren Senna | Koenigsegg |
| McLaren Senna GTR | Koenigsegg |
| Ferrari FXX K | Koenigsegg |
| Koenigsegg Regera | Koenigsegg |
| Lamborghini Aventador | Koenigsegg |

## Overall order and story progression

The installer orders the full catalog from best to worst by the sum of four
menu ratings: **top speed + acceleration + handling + durability**, each from
0 to 9. The maximum overall score is 36. Mass is excluded because more weight
is not consistently better. Original ratings come from the pristine packaged
car data; custom ratings come from the selected manifest. This is a comparison
of the game's ratings, not a measured lap-time or real-world performance claim.

Equal scores keep the originals' relative retail order, followed by custom
cars in manifest order. Every apply recalculates `Cars/Tweak/SortOrder` for all
44 cars without changing physics, ratings, IDs, or model selections. The preview
prints each car's position and score; original cars use their archive aliases.
Changing a custom menu rating can therefore change its position on the next
apply, while editing physics alone does not change its rank.

The original 35 cars retain their original `LOCKED`, `HIDDEN`, and `ADMITTANCE`
requirements byte for byte. With normal story progression enabled, the menu
keeps its retail availability groups: available cars first, then visible locked
cars, each in overall rating order. Hidden locked cars stay hidden. Custom cars
remain available. Reapplying the pack does not reset saved progress.

## Visual editor

Double-click [editor.cmd](editor.cmd), or run it from the repository root:

```powershell
tools\car-mods\editor.cmd
```

To open a specific car and tab directly, for example Panoz's model package:

```powershell
tools\car-mods\editor.cmd --car-id 9004 --tab model
```

Tabs are `specs`, `driving`, `gears`, `advanced` and `model`. The added car IDs are
9004–9006 and 9009–9014. Removed IDs 9001–9003, 9007 and 9008 remain
unused, so the remaining cars keep their existing identities. Older manifests
containing a removed car must have that entry removed before applying.

The editor and installer use `cars.local.json` beside these tools when that
file exists, otherwise [cars.json](cars.json). The local file is ignored by
Git, so your model selections and tuning can stay on this computer. To start
local settings on a fresh checkout, copy `cars.json` to `cars.local.json`
before opening Workshop. An explicit `--config <path>` always selects that
manifest. The tracked nine-car defaults contain no imported model paths;
downloaded sources and prepared model packages are kept outside the release.

The **Car specs** tab opens first and identifies the actual model: the F1
import is a **1995 McLaren F1 GTR**, the Formula 1 import is a **Ferrari F2007**,
the Ford GT is the **2017 model**, and the Aventador is the **LP700-4**. It shows
manufacturer figures with their weight basis and source links. Unknown figures
stay unknown; calibration notes explain the estimates used by the game.
**Restore default driving** clears custom drivetrain and handling overrides,
restoring the shipped weight and power tuning on top of the donor's driving
setup. It keeps model packages, paint, menu ratings and dial limits. Save & Apply
installs it.
The reset values live in [driving_defaults.py](driving_defaults.py), so saving
custom tuning into a manifest does not change the reset target.

Choose a car on the left. Edit its paint colors, menu ratings, physics, gearing,
RPM thresholds and **Interior speedometer maximum** values. The maximum km/h
and mph values set the interior dashboard needle range for added cars. Their
on-screen speedometers keep the donor's native font, artwork, gauge style and
scale. **Original car speedometers…** opens a selector for the original sports
cars, including Koenigsegg, Viper and Lotus. Edit their dial maxima, then choose
**Keep changes** before saving in the main window. Original-car physics stay
unchanged. The paint preview is a
schematic, not a model editor. **Save** writes the settings; **Save & Apply**
also installs them into the game data. Close the game before applying, and
restart it afterward. Invalid settings are rejected before installation, and
the original data backups are retained.

The **Model** tab selects a prepared MM3 model package folder containing
`package.json`. Custom visuals are applied after donor cloning each time, so
editing stats and choosing **Save & Apply** keeps the selected body and
textures. **Use donor model** clears the package selection. Raw MM2 `.ar` and
`.pkg` models require conversion first. A prepared package supplies its own
materials and textures; the schematic paint preview does not show those
materials or guarantee that changing its paint colors recolors the imported
body. The title's contact, suspension, collision and sound systems are retained;
the default extra cars also retain their donor's engine curves and gearing.

The optional per-car `model_package` setting is a folder path relative to
the selected manifest, or an absolute folder path. Existing manifests without this field
continue to use their donor visuals. For example, a Panoz package has:

```json
{
  "format": "mm3-car-model-v1",
  "alias": "Panoz",
  "car_id": 9004,
  "files": [
    {"archive": "loose", "path": "Data/Shared/Cars/Panoz/body.cmp", "source": "body.cmp"},
    {"archive": "Data_hd.zip", "path": "Cars/Panoz/materialSet00.omb", "source": "materialSet00.omb"},
    {"archive": "Data_dvd.zip", "path": "Cars/Panoz/panoz.cdds", "source": "panoz.cdds"}
  ]
}
```

Provide at least `alias` or `car_id`; both, when supplied, must match the
selected car. File paths use forward slashes. Only loose `.cmp` meshes and
archived `.omb` materials or `.cdds` textures in that alias's car/LOD folders
are accepted. Packages may also supply archived menu icons at
`Menu/Gfx/Vehicles/<alias>.cdds` and `<alias>_Small.cdds`, and the associated
`LoadingScreens/Default_<alias>.tga`. The car's selection background is allowed
at the exact loose path `Data/Shared/Cars/<alias>/bg.cdds`. Other loose textures,
other cars' icons, backgrounds and loading screens are
rejected. Source files must remain inside the package and cannot be links
or junctions. Keep the package folder available for future reapplication.
Package assets participate in the normal backup, integrity, rollback and
restore process; restoration removes installed custom visuals without
deleting the prepared source package.

The editor uses the checkout's Python environment and standard Tkinter; it does
not require a web server or additional packages. The code is in
[editor.py](editor.py), and the selected manifest remains editable by hand.

## Command-line installation

From the repository root, with the game closed:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --apply
scripts\play.ps1 -Keyboard
```

The first command previews the changes. The second installs the pack into your
extracted `game_files` data. Use a build containing the custom car name hook so
the extra cars have readable names in the vehicle menu.

Edit the selected `cars.local.json` or [cars.json](cars.json), then run
`prepare.py --apply` again to change their
stats. Menu ratings and driving physics are separate: changing a rating only
changes its menu bar. Physics values control engine torque, mass, drag,
brakes and grip. Keep the car identities unchanged; their IDs and display names
are registered by the runtime. Changes take effect after restarting the game.

Menu ratings use the game's original range, 0 through 9. The main physics
settings are:

| Setting | Changes |
|---|---|
| `TorqueScale` | Native MM3 engine power tuning value in the default donor mode. |
| `Mass` | Physical weight. |
| `BrakeTorque` | Braking strength. |
| `LatFrictionFactor` | Cornering grip. |
| `AirKZZ` | Drag, affecting speed at high engine power. |
| `WheelDrive` | 0: front wheels; 1: rear wheels; 2: all wheels. |
| `GearRatioVector` | Reverse ratio, then one through seven forward ratios. |
| `WheelRatio` | Final drive multiplier: a smaller value makes the gears longer. |
| `TimeChangeGear` | Time spent shifting between forward gears, in seconds. |
| `TimeChangeGearFR` | Time spent changing between forward and reverse, in seconds. |

`GearRatioVector` is a JSON array, such as
`[-3.3, 2.5, 1.72, 1.28, 1.0, 0.82, 0.7]`. The first value must be negative;
the forward ratios must be positive and decrease with each gear. The installer
sets the gear count from the array, so do not add a separate `NGears` field.
One through seven forward gears are supported. A smaller first-gear ratio reduces
the torque sent to the wheels at launch; closer neighboring ratios reduce the
RPM drop during an upshift.

The shipped defaults leave all gear ratios, final drive, shift times and RPM
fields blank, so installation copies them from the pristine donor rather than
keeping a previous modified setup. Formula 1 uses the Lotus's five-speed
transmission and normal RPM range. Regera uses the Koenigsegg's six-speed
transmission. Neither enables the custom factory engine profile. This avoids
the earlier 19,000 RPM Formula 1 setup and single-speed Regera setup.

Weight and power are arcade tuning values. Formula 1 is 600 kg with a torque
scale of 420, compared with its Lotus donor's 1,380 kg and 700; lowering both
weight and engine torque gives brisk acceleration without excessive launch
wheelspin. Regera is 1,590 kg with a torque scale of 1,250, compared with its
donor's 1,526 kg and 1,000. These choices are starting points for driving feel,
not measured acceleration or manufacturer torque claims.

Advanced custom RPM edits are still supported. Keep `MinRPM`,
`MinRPMGearChange`, `CutRPM` and `MaxRPM` in that order from lowest to highest;
`CutRPM` can equal `MaxRPM`. Both peak torque RPM values must stay below the
cutoff. The installer rejects conflicting thresholds. For normal driving,
keep these fields blank and tune only weight and engine power.

The sourced reference catalog is [factory_specs.json](factory_specs.json).
Explicit `engine_profile: "factory"` remains an advanced opt-in for old custom
manifests, with a per-car torque curve and shift target written to
`Data/mm3_factory_physics.ini`. No shipped or imported preset enables it.
Factory profile torque values are in Nm, unlike the native MM3 scale. A default
driving reset removes the opt-in and its drivetrain overrides together.

Select the transmission in the game's car setup. Automatic shifts for you;
Manual requires gear changes. With the MM3 keyboard layout, tap **R** to shift
up and **F** to shift down while driving (Xbox **B** and **X** on a controller).
Automatic calculates its upshift speeds from gearing, wheel radius and the
factory shift target, or `MaxTorqueRPM` (`MaxTurboTorqueRPM` for a turbo-equipped
donor) in donor mode; it checks road speed
to change gear. Wheelspin can therefore raise engine RPM before the car reaches
its upshift speed. Changing `MinRPMGearChange` alone does not change these
automatic upshift speeds.

For example, change Panoz's `physics.TorqueScale` from `750` to `800`
for more engine torque, then reapply the pack. Its acceleration menu bar is
controlled separately by `ratings.ACCELERATION`.

The optional `speedometer` object contains `max_kmh` and `max_mph`, controlling
the added car's interior dashboard needle range separately from driving
performance. The on-screen speedometer retains the donor's native font, artwork,
gauge style and scale. Interior defaults range from 360 km/h / 240 mph for
Panoz to 440 km/h / 280 mph for Regera. These saved ranges do not predict the
tuned car's top speed or change engine power or the menu's `TOPSPEED` rating.
Allowed ranges are 100–1200 km/h and 60–750 mph; both values are required for an
override. Leave both editor fields blank to inherit the donor's interior needle
range.

The optional top-level `original_speedometers` map sets on-screen dial maxima
for original sports cars without changing their physics, paint or interior
camera configuration. The shipped settings raise Koenigsegg, Viper and Lotus
to 800 km/h / 500 mph. For example:

```json
"original_speedometers": {
  "Koenigsegg": {"max_kmh": 1000, "max_mph": 621}
}
```

The installer writes only original-car HUD overrides to
`Data/mm3_speedometers.ini`. Added-car interior ranges stay in the car camera
settings. The sidecar is tracked with the installed assets, rebuilt on reapplication and
removed by restoration. The game reads it at startup, so restart MM3 after
applying changes. An original car with no explicit override uses its retail HUD.

The installer preserves a backup of each affected archive and checks unchanged
resources keep their original bytes and local-header offsets. It never edits
your saved profiles. Cancelling installation, reapplication or restoration with
Ctrl+C restores the previous files and installation metadata. Restore the
pre-install data with:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --restore
```

`--config <path>` selects a different settings file and `--game-dir <path>`
selects another extracted game directory. Reapplication uses the backed-up
originals so tuning does not accumulate. Removing a car from the manifest and
reapplying also removes its installed catalog entries and owned assets; prepared
model packages and pristine archive backups remain available.

To inspect a running native instance without changing game memory:

```powershell
tools\bin\powershell\pwsh.exe -NoProfile -File tools\car-mods\verify.ps1 `
    -ProcessId <pid> -Log <stderr.log> -ExpectedManifest tools\car-mods\cars.json
```

The verifier reports the actual loaded catalog IDs, names and ratings, and
checks the expected extra entries. It also reports the current player's
simulation when a car is active.

Run the editor/installer and optional model-tool tests without starting the game.
Use an interpreter with NumPy, SciPy and Pillow installed for the combined suite,
as described in the [model guide](../car-models/README.md#conversion-requirements).
The smaller build venv does not include all of those optional packages:

```powershell
python -c "import numpy, scipy, PIL; print('Model tools ready')"
python -m unittest discover -s tests -p "test_car*.py"
python -m unittest discover -s tests -p "test_mm3*.py"
python -m unittest discover -s tests -p "test_factory_specs.py"
```

Then activate the native compiler environment for the HUD checks:

```powershell
. .\scripts\enter-env.ps1
python tests/mm3_speedometer_regression.py
python tests/mm3_speedometer_dial_regression.py
```

The HUD checks cover both units, drawn numbers, needle calibration, retail
fallbacks, native rendering arguments and preservation of guest registers.
