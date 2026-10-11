# Extra cars and editable stats

This pack adds three separately selectable tuned variants using the original
game's models: Mini Rally, Mustang GT and Viper Race. The original 35 cars remain
in the catalog. It does not import new 3D models.

From the repository root, with the game closed:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --apply
scripts\play.ps1 -Keyboard
```

The first command previews the changes. The second installs the pack into your
extracted `game_files` data. Use a build containing the custom car name hook so
the three cars have readable names in the vehicle menu.

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

Select the transmission in the game's car setup. Automatic shifts for you;
Manual requires gear changes. With the MM3 keyboard layout, tap **R** to shift
up and **F** to shift down while driving (Xbox **B** and **X** on a controller).
Automatic calculates its upshift speeds from the gearing, wheel radius and
`MaxTorqueRPM` (`MaxTurboTorqueRPM` when turbo is active); it checks road speed
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
