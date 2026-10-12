#!/usr/bin/env python3
"""Small, dependency-free desktop editor for the MM3 extra-car manifest."""

from __future__ import annotations

import argparse
import copy
import csv
import importlib.util
import io
import json
import math
from pathlib import Path
import queue
import re
import subprocess
import sys
import tempfile
import threading
import tkinter as tk
import webbrowser
from tkinter import colorchooser, filedialog, messagebox, ttk

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("mm3_car_prepare", HERE / "prepare.py")
prepare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare)
FACTORY_SPEC = importlib.util.spec_from_file_location("mm3_factory_specs", HERE / "factory_specs.py")
factory = importlib.util.module_from_spec(FACTORY_SPEC)
FACTORY_SPEC.loader.exec_module(factory)
DEFAULT_SPEC = importlib.util.spec_from_file_location("mm3_driving_defaults", HERE / "driving_defaults.py")
defaults = importlib.util.module_from_spec(DEFAULT_SPEC)
DEFAULT_SPEC.loader.exec_module(defaults)

RATING_ORDER = ("TOPSPEED", "ACCELERATION", "MASS", "HANDLING", "DURABILITY")
DRIVING = ("TorqueScale", "Mass", "BrakeTorque", "LatFrictionFactor", "AirKZZ", "WheelDrive")
TRANSMISSION = ("WheelRatio", "TimeChangeGear", "TimeChangeGearFR", "MinRPM", "MinRPMGearChange", "CutRPM", "MaxRPM", "MaxTorqueRPM", "MaxTurboTorqueRPM")
LABELS = {
    "TorqueScale": "Engine torque scale", "Mass": "Mass (kg)", "BrakeTorque": "Braking strength",
    "LatFrictionFactor": "Cornering grip", "AirKZZ": "Air drag", "WheelDrive": "Driven wheels",
    "WheelRatio": "Final drive", "TimeChangeGear": "Shift time (seconds)",
    "TimeChangeGearFR": "Reverse shift time", "MinRPM": "Idle RPM",
    "MinRPMGearChange": "Minimum shift RPM", "CutRPM": "Cutoff RPM", "MaxRPM": "Redline RPM",
    "MaxTorqueRPM": "Peak torque RPM", "MaxTurboTorqueRPM": "Turbo peak torque RPM",
    "TorqueScaleFactor": "Torque multiplier", "HandBrakeTorque": "Handbrake strength",
    "AirKZY": "Downforce scale", "TurboSize": "Donor turbo boost", "GearStatusDownGearFactor": "Downshift factor",
    "SteeringEndSpeed": "Steering end speed", "WheelRadius": "Wheel radius",
}
DRIVE_OPTIONS = ("", "0 - Front", "1 - Rear", "2 - All wheels")


def validated(config: dict) -> dict:
    """Use exactly the same validation as the installer, without saving edits."""
    with tempfile.TemporaryDirectory(prefix="mm3-car-editor-") as temporary:
        path = Path(temporary) / "cars.json"
        path.write_text(json.dumps(config), encoding="utf-8")
        return prepare.load_config(path)


def number(text: str, label: str, integer: bool = False) -> int | float:
    try:
        result = float(text)
    except ValueError as error:
        raise ValueError(f"{label} must be a number") from error
    if not math.isfinite(result) or (integer and result != int(result)):
        raise ValueError(f"{label} must be a finite {'integer' if integer else 'number'}")
    return int(result) if integer or result.is_integer() else result


def edited_car(car: dict, ratings: dict[str, str], physics: dict[str, str],
               gears: str, colors: list[str], speedometer: dict[str, str],
               model_package: str | None = None, factory_engine: bool | None = None) -> dict:
    """Translate the form into config data; blank physics fields inherit the donor."""
    result = copy.deepcopy(car)
    if factory_engine is not None:
        result.pop("engine_profile", None)
        if factory_engine:
            result["engine_profile"] = "factory"
    result["ratings"] = {key: number(ratings[key], key.title(), True) for key in RATING_ORDER}
    result["physics"] = {}
    for key, text in physics.items():
        if text.strip():
            result["physics"][key] = number(text.split(" - ", 1)[0].strip(), LABELS.get(key, key), key == "WheelDrive")
    if gears.strip():
        tokens = re.split(r"[\s,]+", gears.strip().strip("[]").strip())
        result["physics"]["GearRatioVector"] = [number(token, "Gear ratio") for token in tokens]
    result["colors"] = [color.strip().lower() for color in colors]
    result.pop("speedometer", None)
    limits = edited_speedometer(speedometer)
    if limits is not None:
        result["speedometer"] = limits
    if model_package is not None:
        result.pop("model_package", None)
        if model_package.strip():
            result["model_package"] = model_package.strip()
    return result


def edited_speedometer(fields: dict[str, str]) -> dict | None:
    if not any(text.strip() for text in fields.values()):
        return None
    if not all(text.strip() for text in fields.values()):
        raise ValueError("Enter both speedometer limits, or leave both blank to use the original range")
    limits = {key: number(text, "Speedometer limit") for key, text in fields.items()}
    prepare.validate_speedometer(limits)
    return limits


class EditorModel:
    def __init__(self, path: Path):
        self.path = path.resolve()
        self.reload()

    def reload(self) -> None:
        self.config = prepare.load_config(self.path)
        self.baseline = self.path.read_bytes()
        self.saved = copy.deepcopy(self.config)

    def replace_car(self, index: int, car: dict) -> None:
        candidate = copy.deepcopy(self.config)
        candidate["cars"][index] = car
        self.config = validated(candidate)

    def restore_default_driving(self, index: int) -> None:
        self.replace_car(index, defaults.apply_preset(self.config["cars"][index]))

    def replace_original_speedometers(self, settings: dict) -> None:
        candidate = copy.deepcopy(self.config)
        if settings:
            candidate["original_speedometers"] = copy.deepcopy(settings)
        else:
            candidate.pop("original_speedometers", None)
        self.config = validated(candidate)

    def save(self) -> None:
        config = validated(self.config)
        if self.path.read_bytes() != self.baseline:
            raise ValueError(f"{self.path.name} changed outside the editor. Reload it before saving to keep those changes.")
        payload = (json.dumps(config, indent=2) + "\n").encode("utf-8")
        prepare.atomic_write(self.path, payload)
        self.baseline = payload
        self.saved = copy.deepcopy(config)

    def apply_command(self, game_dir: Path) -> list[str]:
        # pythonw has no console streams; use its adjacent console interpreter.
        executable = Path(sys.executable)
        if executable.name.lower() == "pythonw.exe":
            executable = executable.with_name("python.exe")
        return [str(executable), str(HERE / "prepare.py"), "--config", str(self.path),
                "--game-dir", str(game_dir.resolve()), "--apply"]


def game_is_running() -> bool:
    if sys.platform != "win32":
        return False
    result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq mm3_recomp.exe", "/FO", "CSV", "/NH"],
                            capture_output=True, text=True, check=True,
                            creationflags=subprocess.CREATE_NO_WINDOW, timeout=10)
    return any(row and row[0].lower() == "mm3_recomp.exe" for row in csv.reader(io.StringIO(result.stdout)))


class OriginalSpeedometerDialog:
    """Edit retail-car HUD ranges in memory; the main window saves/applies them."""
    def __init__(self, parent: tk.Tk, model: EditorModel, changed, show: bool = True) -> None:
        self.model, self.changed = model, changed
        self.settings = copy.deepcopy(model.config.get("original_speedometers", {}))
        self.aliases = sorted(prepare.ORIGINAL_SPEEDOMETERS, key=lambda alias: prepare.ORIGINAL_SPEEDOMETERS[alias])
        self.current = "Koenigsegg"
        self.window = tk.Toplevel(parent)
        self.window.withdraw()
        self.window.title("Original car speedometers")
        self.window.geometry("455x345")
        self.window.resizable(False, False)
        self.window.transient(parent)
        frame = ttk.Frame(self.window, padding=20)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Original car speedometers", font=("Segoe UI", 15, "bold")).pack(anchor="w")
        ttk.Label(frame, text="Set the on-screen dial maximum for each original sports car.",
                  style="Sub.TLabel", wraplength=410).pack(anchor="w", pady=(5, 12))
        self.selector = ttk.Combobox(frame, state="readonly", width=36,
                                    values=[prepare.ORIGINAL_SPEEDOMETERS[alias] for alias in self.aliases])
        self.selector.pack(fill="x", pady=(0, 12))
        self.selector.bind("<<ComboboxSelected>>", self.select)
        self.speed_vars = {key: tk.StringVar() for key in prepare.SPEEDOMETER}
        for key, unit in (("max_kmh", "km/h"), ("max_mph", "mph")):
            row = ttk.Frame(frame)
            row.pack(fill="x", pady=4)
            low, high = prepare.SPEEDOMETER[key]
            ttk.Label(row, text=f"Maximum {unit}", width=16).pack(side="left")
            ttk.Spinbox(row, from_=low, to=high, increment=10, textvariable=self.speed_vars[key], width=10).pack(side="left")
            ttk.Label(row, text=f"{low}–{high}", style="Sub.TLabel").pack(side="left", padx=12)
        ttk.Label(frame, text="Both blank = original HUD range. This changes the dial scale, not the car's speed or physics.\nUse Save & Apply in the main window after keeping these changes.",
                  style="Sub.TLabel", wraplength=410).pack(anchor="w", pady=(9, 12))
        buttons = ttk.Frame(frame)
        buttons.pack(fill="x", side="bottom")
        ttk.Button(buttons, text="Keep changes", command=self.keep).pack(side="left")
        ttk.Button(buttons, text="Cancel", command=self.window.destroy).pack(side="right")
        self.load(self.current)
        if show:
            self.window.deiconify()
            self.window.grab_set()

    def load(self, alias: str) -> None:
        self.current = alias
        self.selector.current(self.aliases.index(alias))
        for key, variable in self.speed_vars.items():
            variable.set(str(self.settings.get(alias, {}).get(key, "")))

    def collect(self) -> None:
        limits = edited_speedometer({key: variable.get() for key, variable in self.speed_vars.items()})
        if limits is None:
            self.settings.pop(self.current, None)
        else:
            self.settings[self.current] = limits

    def select(self, _event=None) -> None:
        alias = self.aliases[self.selector.current()]
        try:
            self.collect()
        except ValueError as error:
            self.selector.current(self.aliases.index(self.current))
            messagebox.showerror("Check speedometer settings", str(error), parent=self.window)
            return
        self.load(alias)

    def keep(self) -> None:
        try:
            self.collect()
            self.model.replace_original_speedometers(self.settings)
        except (ValueError, OSError) as error:
            messagebox.showerror("Check speedometer settings", str(error), parent=self.window)
            return
        self.changed()
        self.window.destroy()


class CarEditor:
    def __init__(self, root: tk.Tk, config: Path | None = None, game_dir: Path = prepare.ROOT / "game_files"):
        self.root, self.game_dir = root, game_dir
        self.model = EditorModel(config if config is not None else prepare.default_config_path())
        self.current = 0
        self.loading = False
        self.applying = False
        self.events = queue.Queue()
        self.rating_vars, self.physics_vars = {}, {}
        self.physics_fields = {}
        self.gear_var = tk.StringVar()
        self.factory_engine = tk.BooleanVar()
        self.factory_engine.trace_add("write", self._factory_changed)
        self.factory_specs = factory.load_specs()
        self.speed_vars = {key: tk.StringVar() for key in ("max_kmh", "max_mph")}
        self.model_var = tk.StringVar()
        self.model_status = tk.StringVar()
        self.model_var.trace_add("write", self._model_changed)
        self.color_vars = []
        self.preview_color = 0
        self.status = tk.StringVar(value="Choose a car, edit its settings, then save. Apply with the game closed.")
        root.title("MM3 Car Workshop")
        root.geometry("1110x820")
        root.minsize(1030, 790)
        root.configure(background="#f3f5f8")
        root.option_add("*Font", "{Segoe UI} 10")
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure("TFrame", background="#f3f5f8")
        style.configure("TLabel", background="#f3f5f8")
        style.configure("Title.TLabel", font=("Segoe UI", 19, "bold"))
        style.configure("Sub.TLabel", foreground="#526174")
        style.configure("TButton", padding=(10, 7))
        style.configure("Small.TButton", padding=(6, 2))
        self._build()
        self._load_car(0)
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(100, self._poll_apply)

    def _build(self) -> None:
        outer = ttk.Frame(self.root, padding=18)
        outer.pack(fill="both", expand=True)
        ttk.Label(outer, text="Car Workshop", style="Title.TLabel").pack(anchor="w")
        ttk.Label(outer, text="Donor-based driving settings, reference specifications and speedometer ranges.", style="Sub.TLabel").pack(anchor="w", pady=(3, 15))
        # Reserve the actions before the expanding editor panes, so a smaller
        # window never pushes Save/Apply below its visible client area.
        footer = ttk.Frame(outer)
        footer.pack(side="bottom", fill="x")
        body = ttk.Frame(outer)
        body.pack(fill="both", expand=True)
        catalog = ttk.Frame(body, width=205)
        catalog.pack(side="left", fill="y", padx=(0, 16))
        ttk.Label(catalog, text="EXTRA CARS", style="Sub.TLabel").pack(anchor="w", pady=(0, 8))
        self.listbox = tk.Listbox(catalog, width=23, height=17, exportselection=False, borderwidth=0,
                                 highlightthickness=1, highlightbackground="#ccd3df", background="white",
                                 selectbackground="#2066ba", activestyle="none", font=("Segoe UI", 11))
        for car in self.model.config["cars"]:
            self.listbox.insert("end", car["display_name"])
        self.listbox.pack(fill="y", expand=True)
        self.listbox.bind("<<ListboxSelect>>", self.select_car)
        ttk.Label(catalog, text="Blank physics fields use\nthe original donor setting.\n\nRatings only change\nthe vehicle menu bars.", style="Sub.TLabel", wraplength=185).pack(anchor="w", pady=12)
        main = ttk.Frame(body)
        main.pack(side="left", fill="both", expand=True)
        self.name_label = ttk.Label(main, font=("Segoe UI", 15, "bold"))
        self.name_label.pack(anchor="w")
        self.identity_label = ttk.Label(main, style="Sub.TLabel")
        self.identity_label.pack(anchor="w", pady=(3, 12))
        notebook = ttk.Notebook(main)
        self.notebook = notebook
        notebook.pack(fill="both", expand=True)
        driving = ttk.Frame(notebook, padding=14)
        transmission = ttk.Frame(notebook, padding=14)
        advanced = ttk.Frame(notebook, padding=14)
        models = ttk.Frame(notebook, padding=14)
        specs = ttk.Frame(notebook, padding=14)
        self.tabs = {"specs": specs, "driving": driving, "gears": transmission, "advanced": advanced, "model": models}
        notebook.add(specs, text=" Car specs ")
        notebook.add(driving, text=" Driving ")
        notebook.add(transmission, text=" Gears & RPM ")
        notebook.add(advanced, text=" More settings ")
        notebook.add(models, text=" Model ")
        self.spec_title = ttk.Label(specs, font=("Segoe UI", 12, "bold"), wraplength=390)
        self.spec_title.pack(anchor="w", pady=(0, 10))
        spec_body = ttk.Frame(specs)
        spec_body.pack(fill="both", expand=True)
        self.spec_text = tk.Text(spec_body, wrap="word", height=20, width=42, borderwidth=0,
                                 background="#f3f5f8", foreground="#28384b", font=("Segoe UI", 10),
                                 padx=0, pady=0, cursor="arrow")
        spec_scroll = ttk.Scrollbar(spec_body, orient="vertical", command=self.spec_text.yview)
        spec_scroll.pack(side="right", fill="y")
        self.spec_text.configure(yscrollcommand=spec_scroll.set)
        self.spec_text.pack(side="left", fill="both", expand=True)
        self.spec_text.tag_configure("source", foreground="#2066ba", underline=True)
        self.restore_button = ttk.Button(specs, text="Restore default driving", command=self.restore_default_driving)
        self.restore_button.pack(anchor="w", pady=(12, 5))
        ttk.Label(specs, text="Restores donor driving with weight and power tuning. Keeps paint, models, ratings and dial limits. Save & Apply installs the changes.",
                  style="Sub.TLabel", wraplength=390).pack(anchor="w")
        ttk.Label(models, text="Prepared MM3 model package", font=("Segoe UI", 11, "bold")).pack(anchor="w")
        ttk.Label(models, text="Choose a converted model folder containing package.json. The package replaces this car's visuals and survives future Save & Apply changes.",
                  style="Sub.TLabel", wraplength=390).pack(anchor="w", pady=(8, 12))
        self.model_entry = ttk.Entry(models, textvariable=self.model_var)
        self.model_entry.pack(fill="x")
        model_buttons = ttk.Frame(models)
        model_buttons.pack(fill="x", pady=8)
        self.model_browse_button = ttk.Button(model_buttons, text="Choose folder…", command=self.browse_model)
        self.model_browse_button.pack(side="left")
        self.model_clear_button = ttk.Button(model_buttons, text="Use donor model", command=lambda: self.model_var.set(""))
        self.model_clear_button.pack(side="left", padx=8)
        ttk.Label(models, textvariable=self.model_status, style="Sub.TLabel", wraplength=390).pack(anchor="w", pady=(4, 12))
        ttk.Label(models, text="Blank uses the original donor body. Raw MM2 .ar/.pkg files need conversion before they can be selected here. Donor physics, collision and sounds stay in use. A prepared package supplies its own livery and materials; paint controls do not recolor fixed imported textures. Stats and speedometer settings still work.",
                  style="Sub.TLabel", wraplength=390).pack(anchor="w")
        ttk.Label(driving, text="Driving physics", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 8))
        self._physics_fields(driving, DRIVING, 1)
        ttk.Separator(driving).grid(row=7, column=0, columnspan=3, sticky="ew", pady=10)
        ttk.Label(driving, text="Menu ratings (0 to 9)", font=("Segoe UI", 11, "bold")).grid(row=8, column=0, columnspan=3, sticky="w", pady=(0, 8))
        for row, key in enumerate(RATING_ORDER, 9):
            ttk.Label(driving, text=key.replace("TOPSPEED", "TOP SPEED").title()).grid(row=row, column=0, sticky="w", pady=5)
            variable = tk.StringVar()
            self.rating_vars[key] = variable
            ttk.Spinbox(driving, from_=0, to=9, increment=1, width=12, textvariable=variable).grid(row=row, column=1, sticky="w", padx=10)
        self._physics_fields(transmission, TRANSMISSION, 0)
        ttk.Label(transmission, text="Gear ratios").grid(row=9, column=0, sticky="w", pady=(7, 3))
        ttk.Entry(transmission, textvariable=self.gear_var, width=37).grid(row=10, column=0, columnspan=3, sticky="ew")
        ttk.Label(transmission, text="Leave blank to keep the donor's gearbox. Custom ratios: reverse first, then 1–7 decreasing forward ratios.", style="Sub.TLabel", wraplength=390).grid(row=11, column=0, columnspan=3, sticky="w", pady=4)
        ttk.Label(transmission, text="RPM order: idle < shift < cutoff ≤ redline", style="Sub.TLabel").grid(row=12, column=0, columnspan=3, sticky="w", pady=4)
        ttk.Label(transmission, text="Factory mode uses a fixed torque-curve shape and shift target; peak torque RPM is a reference. Donor mode uses the editable peak RPM fields. Wheel geometry determines rolling radius.", style="Sub.TLabel", wraplength=390).grid(row=13, column=0, columnspan=3, sticky="w", pady=(4, 0))
        ttk.Checkbutton(transmission, text="Use factory engine profile", variable=self.factory_engine).grid(row=14, column=0, columnspan=3, sticky="w", pady=(12, 0))
        ttk.Label(transmission, text="Factory mode: engine torque scale is in Nm. Donor mode: it is an MM3 tuning value.", style="Sub.TLabel", wraplength=390).grid(row=15, column=0, columnspan=3, sticky="w", pady=(4, 0))
        remaining = [key for key in prepare.PHYSICS if key not in DRIVING + TRANSMISSION]
        self._physics_fields(advanced, remaining, 0)
        preview = ttk.Frame(body, width=245)
        preview.pack(side="right", fill="y", padx=(16, 0))
        ttk.Label(preview, text="PAINT PREVIEW", style="Sub.TLabel").pack(anchor="w", pady=(0, 8))
        self.canvas = tk.Canvas(preview, width=245, height=145, background="#172638", highlightthickness=0)
        self.canvas.pack()
        ttk.Label(preview, text="Body schematic • colors only", style="Sub.TLabel", wraplength=245).pack(anchor="w", pady=(4, 5))
        self.paint_frame = ttk.Frame(preview)
        self.paint_frame.pack(fill="x")
        palette_buttons = ttk.Frame(preview)
        palette_buttons.pack(fill="x", pady=4)
        self.add_button = ttk.Button(palette_buttons, text="+ Paint", command=self.add_color, style="Small.TButton")
        self.add_button.pack(side="left")
        self.remove_button = ttk.Button(palette_buttons, text="− Paint", command=self.remove_color, style="Small.TButton")
        self.remove_button.pack(side="right")
        ttk.Separator(preview).pack(fill="x", pady=7)
        ttk.Label(preview, text="Speedometer maximum", font=("Segoe UI", 11, "bold")).pack(anchor="w")
        self.gauge = tk.Canvas(preview, width=245, height=118, background="#f3f5f8", highlightthickness=0)
        self.gauge.pack(pady=3)
        limits = getattr(prepare, "SPEEDOMETER", {"max_kmh": (100, 1200), "max_mph": (60, 750)})
        for key, unit in (("max_kmh", "km/h"), ("max_mph", "mph")):
            row = ttk.Frame(preview)
            row.pack(fill="x", pady=2)
            ttk.Label(row, text=f"Max {unit}", width=11).pack(side="left")
            low, high = limits[key]
            ttk.Spinbox(row, from_=low, to=high, increment=10, textvariable=self.speed_vars[key], width=10).pack(side="left")
            self.speed_vars[key].trace_add("write", self._draw)
        ttk.Label(preview, text=f"On-screen dial and interior needle.\nBoth blank = original range.\nkm/h {limits['max_kmh'][0]}–{limits['max_kmh'][1]}; mph {limits['max_mph'][0]}–{limits['max_mph'][1]}.", style="Sub.TLabel", wraplength=245).pack(anchor="w", pady=4)
        self.original_speed_button = ttk.Button(preview, text="Original car speedometers…", command=self.original_speedometers,
                                               style="Small.TButton")
        self.original_speed_button.pack(fill="x", pady=(4, 0))
        ttk.Separator(footer).pack(fill="x", pady=(10, 8))
        buttons = ttk.Frame(footer)
        buttons.pack(fill="x")
        self.save_button = ttk.Button(buttons, text="Save settings", command=self.save)
        self.save_button.pack(side="left")
        self.apply_button = ttk.Button(buttons, text="Save & Apply to game", command=self.apply)
        self.apply_button.pack(side="left", padx=8)
        self.reload_button = ttk.Button(buttons, text="Reload settings", command=self.reload)
        self.reload_button.pack(side="left")
        ttk.Button(buttons, text="Close", command=self.close).pack(side="right")
        ttk.Label(footer, textvariable=self.status, style="Sub.TLabel", wraplength=980).pack(anchor="w", pady=(8, 0))

    def _physics_fields(self, parent, keys, start) -> None:
        for row, key in enumerate(keys, start):
            ttk.Label(parent, text=LABELS.get(key, key)).grid(row=row, column=0, sticky="w", pady=2)
            variable = tk.StringVar()
            self.physics_vars[key] = variable
            if key == "WheelDrive":
                field = ttk.Combobox(parent, textvariable=variable, values=DRIVE_OPTIONS, state="readonly", width=17)
            else:
                field = ttk.Entry(parent, textvariable=variable, width=19)
            field.grid(row=row, column=1, padx=(10, 6), sticky="w")
            self.physics_fields[key] = field
            low, high = prepare.PHYSICS[key]
            ttk.Label(parent, text=f"{low:g}–{high:g}", style="Sub.TLabel").grid(row=row, column=2, sticky="w")

    def _factory_changed(self, *_args) -> None:
        for key in ("MaxTorqueRPM", "MaxTurboTorqueRPM", "TurboSize"):
            if key in self.physics_fields:
                self.physics_fields[key].configure(state="readonly" if self.factory_engine.get() else "normal")

    def _load_car(self, index: int) -> None:
        self.loading = True
        self.current = index
        car = self.model.config["cars"][index]
        self.name_label.configure(text=car["display_name"])
        self.identity_label.configure(text=f"ID {car['id']} · {car['donor']} donor setup")
        self.factory_engine.set(car.get("engine_profile") == "factory")
        self._show_specs(car)
        self.model_var.set(car.get("model_package", ""))
        for key, variable in self.rating_vars.items():
            variable.set(str(car["ratings"][key]))
        for key, variable in self.physics_vars.items():
            value = car["physics"].get(key, "")
            variable.set(DRIVE_OPTIONS[value + 1] if key == "WheelDrive" and value != "" else str(value))
        self.gear_var.set(", ".join(str(value) for value in car["physics"].get("GearRatioVector", [])))
        for key, variable in self.speed_vars.items():
            variable.set(str(car.get("speedometer", {}).get(key, "")))
        self._paint_rows(car["colors"])
        self.listbox.selection_clear(0, "end")
        self.listbox.selection_set(index)
        self.listbox.see(index)
        self.loading = False
        self._draw()

    def _show_specs(self, car: dict) -> None:
        reference = self.factory_specs[car["alias"]]
        spec = reference["specification"]
        self.spec_title.configure(text=reference["variant"])
        self.spec_text.configure(state="normal")
        self.spec_text.delete("1.0", "end")
        lines = [f"Power: {spec.get('power_label') or 'Not published'}"]
        torque = spec.get("torque_nm")
        lines.append(f"Torque: {torque:g} Nm" if torque is not None else f"Torque: {spec.get('torque_label') or 'not published / unresolved'}")
        mass = spec.get("mass_kg")
        lines.append(f"Mass: {mass:g} kg ({spec.get('mass_basis', 'reference')})" if mass is not None else "Mass: not published")
        speed = spec.get("top_speed_kmh")
        lines.append(f"Top speed: {speed:g} km/h ({speed / 1.609344:.0f} mph)" if speed is not None else "Top speed: not published; circuit dependent")
        acceleration = spec.get("zero_to_100_s")
        lines.append(f"0–100 km/h: {acceleration:g} seconds" if acceleration is not None else "0–100 km/h: not published")
        lines.append(f"Transmission: {spec['gear_count']} forward gear(s)")
        lines.append(spec.get("drivetrain", ""))
        self.spec_text.insert("end", "\n".join(lines) + "\n\n")
        self.spec_text.insert("end", "Workshop defaults keep donor gears, RPM, grip and brakes; only weight and engine power are tuned. The specifications and factory calibration notes here are reference information.\n\nFactory reference notes\n" + "\n".join(reference.get("notes", [])) + "\n\nSources\n")
        for index, source in enumerate(reference.get("sources", [])):
            tag = f"source_{index}"
            self.spec_text.insert("end", source["title"] + "\n", ("source", tag))
            self.spec_text.tag_bind(tag, "<Button-1>", lambda _event, url=source["url"]: webbrowser.open(url))
        self.spec_text.configure(state="disabled")

    def restore_default_driving(self) -> None:
        try:
            # Tuning may be invalid while typing; the preset replaces it.
            # Preserve current art, menu ratings and gauges without parsing
            # in-progress transmission or handling fields.
            car = copy.deepcopy(self.model.config["cars"][self.current])
            car["colors"] = [variable.get().strip().lower() for variable in self.color_vars]
            car["ratings"] = {key: number(variable.get(), key, True)
                              for key, variable in self.rating_vars.items()}
            car.pop("speedometer", None)
            gauges = edited_speedometer({key: variable.get() for key, variable in self.speed_vars.items()})
            if gauges is not None:
                car["speedometer"] = gauges
            car.pop("model_package", None)
            if self.model_var.get().strip():
                car["model_package"] = self.model_var.get().strip()
            self.model.replace_car(self.current, defaults.apply_preset(car))
            self._load_car(self.current)
        except (ValueError, OSError) as error:
            self._error(error)
            return
        self.status.set("Default donor driving restored for this car. Save & Apply to install it.")

    def select_initial(self, car_id: int | None, tab: str) -> None:
        """Optional launch selection; ordinary launches still start at car one."""
        if car_id is not None:
            index = next((index for index, car in enumerate(self.model.config["cars"])
                          if car["id"] == car_id), None)
            if index is None:
                raise ValueError(f"Car ID {car_id} is not present in this settings file")
            self._load_car(index)
        self.notebook.select(self.tabs[tab])

    def _paint_rows(self, colors: list[str]) -> None:
        for child in self.paint_frame.winfo_children():
            child.destroy()
        self.color_vars = []
        self.preview_color = min(self.preview_color, len(colors) - 1)
        for index, color in enumerate(colors):
            variable = tk.StringVar(value=color)
            variable.trace_add("write", self._draw)
            self.color_vars.append(variable)
            row = ttk.Frame(self.paint_frame)
            row.pack(fill="x", pady=2)
            ttk.Label(row, text=f"Paint {index + 1}", width=8).pack(side="left")
            entry = ttk.Entry(row, textvariable=variable, width=11)
            entry.pack(side="left", padx=4)
            entry.bind("<FocusIn>", lambda _event, i=index: self.preview_paint(i))
            ttk.Button(row, text="Pick…", command=lambda i=index: self.pick_color(i), style="Small.TButton").pack(side="right")
        self.add_button.configure(state="normal" if len(colors) < 3 else "disabled")
        self.remove_button.configure(state="normal" if len(colors) > 1 else "disabled")

    def _draw(self, *_args) -> None:
        if self.loading or not hasattr(self, "gauge"):
            return
        colors = [variable.get() if re.fullmatch(r"#[0-9a-fA-F]{6}", variable.get()) else "#888888" for variable in self.color_vars]
        color = colors[self.preview_color] if colors else "#888888"
        c = self.canvas
        c.delete("all")
        c.create_text(16, 19, text=f"Paint {self.preview_color + 1}", anchor="w", fill="#d8e4f3", font=("Segoe UI", 9))
        c.create_oval(23, 128, 227, 155, fill="#101c29", outline="")
        for x in (48, 174):
            c.create_oval(x, 112, x + 30, 144, fill="#09121a", outline="#657488", width=2)
            c.create_oval(x + 8, 120, x + 22, 135, fill="#a1aeba", outline="")
        c.create_polygon(22, 116, 36, 92, 75, 87, 94, 63, 154, 63, 181, 88, 217, 98, 224, 119,
                         208, 127, 32, 127, fill=color, outline="#edf5ff", width=2, smooth=True)
        c.create_polygon(92, 69, 117, 69, 117, 89, 77, 89, fill="#7bc1e5", outline="#25344a")
        c.create_polygon(123, 69, 151, 69, 173, 89, 123, 89, fill="#7bc1e5", outline="#25344a")
        c.create_line(123, 95, 123, 121, fill="#405164", width=2)
        c.create_line(134, 98, 145, 98, fill="#edf5ff", width=2)
        c.create_rectangle(211, 104, 220, 113, fill="#f9ecc0", outline="")
        for index, swatch in enumerate(colors):
            item = c.create_oval(84 + index * 29, 162, 101 + index * 29, 179, fill=swatch, outline="white", width=3 if index == self.preview_color else 1)
            c.tag_bind(item, "<Button-1>", lambda _event, i=index: self.preview_paint(i))
        c.scale("all", 0, 0, 1, 0.78)
        g = self.gauge
        g.delete("all")
        g.create_arc(41, 8, 205, 172, start=0, extent=180, style="arc", outline="#bac7d8", width=9)
        limit = self.speed_vars["max_kmh"].get().strip()
        try:
            maximum = float(limit)
            if not math.isfinite(maximum) or maximum <= 0:
                maximum = None
        except ValueError:
            maximum = None
        for index in range(5):
            angle = math.pi - index * math.pi / 4
            x, y = 123 + 82 * math.cos(angle), 90 - 82 * math.sin(angle)
            g.create_line(x, y, 123 + 70 * math.cos(angle), 90 - 70 * math.sin(angle), fill="#526174", width=2)
            if maximum is not None:
                g.create_text(123 + 58 * math.cos(angle), 90 - 58 * math.sin(angle), text=f"{maximum * index / 4:g}", font=("Segoe UI", 8), fill="#526174")
        g.create_line(123, 90, 163, 42, fill="#2066ba", width=3)
        g.create_oval(118, 85, 128, 95, fill="#2066ba", outline="")
        g.create_text(123, 109, text=f"{limit} km/h" if maximum else "Donor dial range", fill="#526174", font=("Segoe UI", 10))

    def preview_paint(self, index: int) -> None:
        self.preview_color = index
        self._draw()

    def original_speedometers(self) -> None:
        OriginalSpeedometerDialog(self.root, self.model,
                                  lambda: self.status.set("Original speedometer changes kept. Save & Apply to install them, then restart MM3."))

    def pick_color(self, index: int) -> None:
        self.preview_paint(index)
        current = self.color_vars[index].get()
        chosen = colorchooser.askcolor(current if re.fullmatch(r"#[0-9a-fA-F]{6}", current) else "#888888", parent=self.root, title=f"Paint {index + 1}")[1]
        if chosen:
            self.color_vars[index].set(chosen)

    def add_color(self) -> None:
        self._paint_rows([variable.get() for variable in self.color_vars] + ["#ffffff"])
        self._draw()

    def _model_changed(self, *_args) -> None:
        folder = self.model_var.get().strip()
        if not folder:
            self.model_status.set("Using the original donor model.")
            return
        path = Path(folder)
        if not path.is_absolute():
            path = self.model.path.parent / path
        self.model_status.set("Package selected; files and target car are checked before applying."
                              if (path / "package.json").is_file()
                              else "package.json is missing from this folder. Choose a prepared MM3 model package.")

    def browse_model(self) -> None:
        folder = filedialog.askdirectory(parent=self.root, title="Choose prepared MM3 model package",
                                         initialdir=str(self.model.path.parent), mustexist=True)
        if not folder:
            return
        try:
            car = {**self.model.config["cars"][self.current], "model_package": folder}
            prepare.load_model_package(car, self.model.path.parent)
        except (ValueError, OSError) as error:
            self._error(error)
            return
        self.model_var.set(folder)

    def remove_color(self) -> None:
        self._paint_rows([variable.get() for variable in self.color_vars][:-1])
        self._draw()

    def _collect(self) -> None:
        car = edited_car(self.model.config["cars"][self.current],
                         {key: variable.get() for key, variable in self.rating_vars.items()},
                         {key: variable.get() for key, variable in self.physics_vars.items()},
                         self.gear_var.get(), [variable.get() for variable in self.color_vars],
                         {key: variable.get() for key, variable in self.speed_vars.items()}, self.model_var.get(),
                         factory_engine=self.factory_engine.get())
        self.model.replace_car(self.current, car)

    def _error(self, error) -> None:
        self.status.set(str(error))
        messagebox.showerror("Check car settings", str(error), parent=self.root)

    def select_car(self, _event=None) -> None:
        selection = self.listbox.curselection()
        if self.loading or not selection or selection[0] == self.current:
            return
        try:
            self._collect()
        except (ValueError, OSError) as error:
            self.listbox.selection_clear(0, "end")
            self.listbox.selection_set(self.current)
            self._error(error)
            return
        self._load_car(selection[0])

    def save(self) -> bool:
        try:
            self._collect()
            self.model.save()
        except (ValueError, OSError) as error:
            self._error(error)
            return False
        self.status.set("Settings saved. Apply them to the game, then restart it to load the changes.")
        return True

    def reload(self) -> None:
        if not messagebox.askyesno("Reload settings", "Discard edits in this window and reload the settings file?", parent=self.root):
            return
        try:
            self.model.reload()
            self.listbox.delete(0, "end")
            for car in self.model.config["cars"]:
                self.listbox.insert("end", car["display_name"])
            self._load_car(min(self.current, len(self.model.config["cars"]) - 1))
            self.status.set("Reloaded settings from disk.")
        except (ValueError, OSError) as error:
            self._error(error)

    def apply(self) -> None:
        if self.applying or not self.save():
            return
        try:
            if game_is_running():
                self.status.set("Settings saved. Close the running MM3 game, then click Save & Apply again.")
                return
        except (OSError, subprocess.SubprocessError) as error:
            self._error(f"Unable to check whether the game is closed: {error}")
            return
        self.applying = True
        for button in (self.save_button, self.apply_button, self.reload_button, self.original_speed_button, self.restore_button,
                       self.model_browse_button, self.model_clear_button, self.model_entry):
            button.configure(state="disabled")
        self.status.set("Applying settings… Original game-data backups are preserved by the installer.")
        command = self.model.apply_command(self.game_dir)

        def worker():
            try:
                result = subprocess.run(command, capture_output=True, text=True,
                                        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0)
                self.events.put((result.returncode, result.stdout, result.stderr))
            except OSError as error:
                self.events.put((1, "", str(error)))

        threading.Thread(target=worker, daemon=True).start()

    def _poll_apply(self) -> None:
        try:
            code, stdout, stderr = self.events.get_nowait()
        except queue.Empty:
            pass
        else:
            self.applying = False
            for button in (self.save_button, self.apply_button, self.reload_button, self.original_speed_button, self.restore_button,
                           self.model_browse_button, self.model_clear_button, self.model_entry):
                button.configure(state="normal")
            if code:
                self._error((stderr or stdout).strip() or "The installer did not complete.")
            else:
                self.status.set("Applied successfully. Restart MM3 to load the cars, paint and new stats.")
        self.root.after(100, self._poll_apply)

    def close(self) -> None:
        if self.applying:
            self.status.set("Wait for the installation to finish before closing this window.")
            return
        try:
            self._collect()
            dirty = self.model.config != self.model.saved
        except (ValueError, OSError):
            dirty = True
        if dirty:
            answer = messagebox.askyesnocancel("Unsaved settings", "Save your changes before closing?", parent=self.root)
            if answer is None or (answer and not self.save()):
                return
        self.root.destroy()


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=prepare.default_config_path(),
                        help="Manifest path (default: cars.local.json when present, otherwise cars.json)")
    parser.add_argument("--game-dir", type=Path, default=prepare.ROOT / "game_files")
    parser.add_argument("--car-id", type=int, choices=tuple(prepare.IDENTITIES), help="Select this car when opening the workshop")
    parser.add_argument("--tab", choices=("specs", "driving", "gears", "advanced", "model"), default="specs",
                        help="Open this settings tab (default: specs)")
    parser.add_argument("--smoke-test", action="store_true", help="Open, verify and close the UI without saving or applying")
    return parser.parse_args(argv)


def main() -> int:
    args = parse_args()
    root = tk.Tk()
    if args.smoke_test:
        root.withdraw()
    try:
        app = CarEditor(root, args.config, args.game_dir)
        app.select_initial(args.car_id, args.tab)
        if args.smoke_test:
            root.update()
            if args.car_id is not None:
                assert app.model.config["cars"][app.current]["id"] == args.car_id
            assert app.notebook.select() == str(app.tabs[args.tab])
            print(f"Initial selection checked: car {app.model.config['cars'][app.current]['id']}, tab {args.tab}.")
            for index in range(len(app.model.config["cars"])):
                app._load_car(index)
                app._collect()
                root.update()
            dialog = OriginalSpeedometerDialog(root, app.model, lambda: None, show=False)
            for alias in dialog.aliases:
                dialog.load(alias)
                dialog.collect()
                root.update()
            dialog.keep()
            print(f"Car editor UI checked: {len(app.model.config['cars'])} added cars, "
                  f"{len(dialog.aliases)} original speedometers, {len(app.physics_vars)} physics fields.")
            root.destroy()
        else:
            root.mainloop()
        return 0
    except (ValueError, OSError) as error:
        messagebox.showerror("MM3 Car Workshop", str(error), parent=root)
        root.destroy()
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
