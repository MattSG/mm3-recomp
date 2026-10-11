"""Sourced vehicle facts and explicitly estimated MM3 calibration presets.

This catalog is deliberately separate from the strict installation manifest.
Unknown factory figures remain None; the game's necessary approximations live
in calibration/notes. Applying a preset never installs assets or changes paint.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path

CATALOG_PATH = Path(__file__).with_name("factory_specs.json")


def load_specs() -> dict:
    """Return a fresh alias-keyed reference catalog (callers may edit their copy)."""
    data = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    if data.get("version") != 1 or not isinstance(data.get("cars"), dict):
        raise ValueError("Unsupported factory specification catalog")
    return data["cars"]


def apply_preset(car: dict) -> dict:
    """Replace tuning/gauges, opt in to the runtime engine, preserve identity/art."""
    specs = load_specs()
    alias = car.get("alias")
    if alias not in specs:
        raise ValueError(f"No factory reference preset for {alias!r}")
    result = copy.deepcopy(car)
    for field in ("physics", "ratings", "speedometer"):
        result[field] = copy.deepcopy(specs[alias][field])
    result["engine_profile"] = "factory"
    return result


def apply_presets(config: dict) -> dict:
    """Apply only to existing catalog slots; never resurrect a removed vehicle."""
    result = copy.deepcopy(config)
    result["cars"] = [apply_preset(car) for car in config["cars"]]
    return result


def profile_for(car: dict) -> dict | None:
    """Resolve the explicit opt-in; never infer it from a familiar alias."""
    if "engine_profile" not in car:
        return None
    if car["engine_profile"] != "factory":
        raise ValueError("engine_profile must be 'factory', or omitted for retail physics")
    entry = load_specs().get(car.get("alias"))
    if entry is None:
        raise ValueError("Factory engine profile requires a supported catalog alias")
    return entry["calibration"]


def factory_physics_settings(config: dict) -> bytes:
    """Native sidecar: alias shift_rpm radius count [rpm fraction]... ."""
    lines = ["# MM3 factory physics v1: alias shift_rpm reference_wheel_radius point_count rpm fraction ..."]
    for car in config["cars"]:
        profile = profile_for(car)
        if profile is None:
            continue
        curve = profile["torque_curve"]
        values = [profile["shift_rpm"], profile["reference_wheel_radius_m"], len(curve)]
        values.extend(value for point in curve for value in point)
        lines.append(car["alias"] + " " + " ".join(format(value, ".9g") for value in values))
    return ("\n".join(lines) + "\n").encode("ascii")
