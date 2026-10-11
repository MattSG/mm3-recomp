# Extra cars and editable stats

Car Workshop supports fourteen separately selectable extra-car identities.
The original 35 cars remain in the catalog. Each variant has its own name,
ratings and driving setup, with an original MM3 donor providing physics,
interior, sound and collision. A selected prepared model package supplies its
custom body, wheels, textures and livery; a blank package selection uses the
donor visuals. These are game-tuned interpretations, rather than measured
real-world specifications.

| Added car | Original MM3 donor |
|---|---|
| Mini Rally | Mini |
| Mustang GT | Mustang Fastback |
| Viper Race | Dodge Viper |
| Panoz GTR-1 | Dodge Viper |
| McLaren F1 | Koenigsegg |
| Formula 1 | Lotus |
| Bugatti Chiron | Koenigsegg |
| Bugatti Bolide | Koenigsegg |
| Ford GT | Koenigsegg |
| McLaren Senna | Koenigsegg |
| McLaren Senna GTR | Koenigsegg |
| Ferrari FXX K | Koenigsegg |
| Koenigsegg Regera | Koenigsegg |
| Lamborghini Aventador | Koenigsegg |

## Visual editor

Double-click [editor.cmd](editor.cmd), or run it from the repository root:

```powershell
tools\car-mods\editor.cmd
```

To open a specific car and tab directly, for example Panoz's model package:

```powershell
tools\car-mods\editor.cmd --car-id 9004 --tab model
```

Tabs are `driving`, `gears`, `advanced` and `model`. The added car IDs are
9001–9014; the five new entries use 9010 (Senna), 9011 (Senna GTR), 9012 (FXX K),
9013 (Regera) and 9014 (Aventador). Older nine-car manifests remain valid.

The editor and installer use `cars.local.json` beside these tools when that
file exists, otherwise [cars.json](cars.json). The local file is ignored by
Git, so your model selections and tuning can stay on this computer. To start
local settings on a fresh checkout, copy `cars.json` to `cars.local.json`
before opening Workshop. An explicit `--config <path>` always selects that
manifest. The tracked fourteen-car defaults contain no imported model paths;
downloaded sources and prepared model packages are kept outside the release.

Choose a car on the left. Edit its paint colors, menu ratings, physics, gearing,
RPM thresholds and **Speedometer maximum** values. The maximum km/h and mph
values set both the on-screen dial scale and the interior needle range for added
cars. **Original car speedometers…** opens a selector for the original sports
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
body. Donor physics, collision and sounds are retained.

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
`LoadingScreens/Default_<alias>.tga`. Other cars' icons and loading screens are
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
| `TorqueScale` | Engine power and acceleration. |
| `Mass` | Physical weight. |
| `BrakeTorque` | Braking strength. |
| `LatFrictionFactor` | Cornering grip. |
| `AirKZZ` | Drag, affecting speed at high engine power. |
| `WheelDrive` | 0: front wheels; 1: rear wheels; 2: all wheels. |
| `GearRatioVector` | Reverse ratio, then the ratios for gears 1 through 6. |
| `WheelRatio` | Final drive multiplier: a smaller value makes the gears longer. |
| `TimeChangeGear` | Time spent shifting between forward gears, in seconds. |
| `TimeChangeGearFR` | Time spent changing between forward and reverse, in seconds. |

`GearRatioVector` is a JSON array, such as
`[-3.3, 2.5, 1.72, 1.28, 1.0, 0.82, 0.7]`. The first value must be negative;
the forward ratios must be positive and decrease with each gear. The installer
sets the gear count from the array, so do not add a separate `NGears` field.
Two through six forward gears are supported. A smaller first-gear ratio reduces
the torque sent to the wheels at launch; closer neighboring ratios reduce the
RPM drop during an upshift.

RPM settings can also be edited with `MinRPM`, `MinRPMGearChange`, `CutRPM` and
`MaxRPM`. Keep them in that order from lowest to highest; `CutRPM` can equal
`MaxRPM`. The installer rejects invalid ratios and conflicting RPM thresholds.
Both peak torque RPM values must also stay below the cutoff, including inherited
donor values, and cannot exceed 8000 RPM with the supported stock torque curve.
MM3's stock torque curve does not support a modern Formula 1 RPM
range, so the Formula 1 variant uses RPM values that the native engine can drive
through its automatic shift points.

Select the transmission in the game's car setup. Automatic shifts for you;
Manual requires gear changes. With the MM3 keyboard layout, tap **R** to shift
up and **F** to shift down while driving (Xbox **B** and **X** on a controller).
Automatic calculates its upshift speeds from the gearing, wheel radius and
`MaxTorqueRPM` (`MaxTurboTorqueRPM` for a turbo-equipped donor); it checks road speed
to change gear. Wheelspin can therefore raise engine RPM before the car reaches
its upshift speed. Changing `MinRPMGearChange` alone does not change these
automatic upshift speeds.

Viper Race uses a longer first gear and closer early gear ratios to suit its
higher torque and lower weight. Its final drive is `3.8`, and forward shifts
take `0.30` seconds, compared with the original Viper's `4.0` final drive and
`0.42` seconds. The six forward ratios are
`2.50 / 1.72 / 1.28 / 1.00 / 0.82 / 0.70`; reverse remains `-3.3`.

For example, change Mini Rally's `physics.TorqueScale` from `440` to `500`
for more engine torque, then reapply the pack. Its acceleration menu bar is
controlled separately by `ratings.ACCELERATION`.

The optional `speedometer` object contains `max_kmh` and `max_mph`, controlling
the on-screen dial maximum and the interior dashboard needle range separately
from driving performance. The fast variants use 800 km/h and 500 mph; Mini Rally
uses 500 km/h and 311 mph. The dial labels and needle use the selected maximum
in the game's current units. Numeric speed below the dial continues above that
maximum. Raising the range does not add engine power or change the menu's
`TOPSPEED` rating. Allowed ranges are 100–1200 km/h and 60–750 mph; both values
are required for an override. Leave both editor fields blank to restore the
original HUD scale and inherit the donor's interior needle range.

The optional top-level `original_speedometers` map sets on-screen dial maxima
for original sports cars without changing their physics, paint or interior
camera configuration. The shipped settings raise Koenigsegg, Viper and Lotus
to 800 km/h / 500 mph. For example:

```json
"original_speedometers": {
  "Koenigsegg": {"max_kmh": 1000, "max_mph": 621}
}
```

The installer writes the configured ranges to `Data/mm3_speedometers.ini`.
This file is tracked with the installed assets, rebuilt on reapplication and
removed by restoration. The game reads it at startup, so restart MM3 after
applying changes. A car with no explicit override uses its original HUD.

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
originals so tuning does not accumulate.

To inspect a running native instance without changing game memory:

```powershell
tools\bin\powershell\pwsh.exe -NoProfile -File tools\car-mods\verify.ps1 `
    -ProcessId <pid> -Log <stderr.log> -ExpectedManifest tools\car-mods\cars.json
```

The verifier reports the actual loaded catalog IDs, names and ratings, and
checks the expected extra entries. It also reports the current player's
simulation when a car is active.

Run the editor/installer and native HUD checks without starting the game:

```powershell
. .\scripts\enter-env.ps1
python -m unittest discover -s tests -p "test_car*.py"
python tests/mm3_speedometer_regression.py
python tests/mm3_speedometer_dial_regression.py
```

The HUD checks cover both units, drawn numbers, needle calibration, retail
fallbacks, native rendering arguments and preservation of guest registers.
