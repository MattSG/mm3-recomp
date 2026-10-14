"""Arcade weight/power presets layered onto pristine retail donor physics.

TorqueScale values are native MM3 tuning units, not manufacturer Nm. Lower
weights use moderate torque to limit wheelspin with the donor's road-speed
automatic shifting. Everything except weight and engine power is inherited.
"""
from __future__ import annotations

import copy

# Alias: (mass in kg, native MM3 torque scale). Keep reset presets separate from
# editable manifests so saving custom tuning cannot overwrite the reset target.
TUNING = {
    "Panoz": (980, 750),
    "McLaren_F1": (1050, 800),
    "Form1": (600, 420),
    "Ford_GTcar": (1520, 1000),
    "Senna_Road": (1309, 1000),
    "Senna_GTR1": (1270, 1025),
    "FXXK_Racer": (1590, 1150),
    "Regera_GT1": (1590, 1250),
    "AventadorX": (1675, 1100),
}


def apply_preset(car: dict) -> dict:
    """Reset driving only; preserve identity, model, paint, ratings and gauges."""
    alias = car.get("alias")
    if alias not in TUNING:
        raise ValueError(f"No default driving setup for {alias!r}")
    result = copy.deepcopy(car)
    mass, torque = TUNING[alias]
    result["physics"] = {"Mass": mass, "TorqueScale": torque}
    result.pop("engine_profile", None)
    return result
