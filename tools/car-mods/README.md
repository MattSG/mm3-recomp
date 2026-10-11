# Extra cars and editable stats

This pack adds nine separately selectable tuned variants. The original 35 cars
remain in the catalog. Each variant has its own name, paint, ratings and driving
setup, while reusing an original MM3 body, interior, engine sound and collision
model. These are game-tuned interpretations, not replicas or measured real-world
specifications; Formula 1 currently uses the enclosed Lotus body.

| Added car | MM3 body |
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

## Visual editor

Double-click [editor.cmd](editor.cmd), or run it from the repository root:

```powershell
tools\car-mods\editor.cmd
```

Choose a car on the left. Edit its paint colors, menu ratings, physics, gearing,
RPM thresholds and speedometer limits in the tabs. The paint preview is a
schematic, not a model editor. **Save** writes the settings; **Save & Apply**
also installs them into the game data. Close the game before applying, and
restart it afterward. Invalid settings are rejected before installation, and
the original data backups are retained.

The editor uses the checkout's Python environment and standard Tkinter; it does
not require a web server or additional packages. The code is in
[editor.py](editor.py), and [cars.json](cars.json) remains editable by hand.

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

Edit [cars.json](cars.json), then run `prepare.py --apply` again to change their
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
the interior dashboard needle range separately from driving performance. The
fast variants use 800 km/h and 500 mph. The full-screen sport and Koenigsegg HUDs
also show numeric speed below the dial, using the game's selected units and
continuing beyond the analog dial's highest marking. The analog needle retains
its original calibration. Raising a dashboard range does not add engine power
or change the menu's `TOPSPEED` rating.

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
