"""Run production console-rate wrappers against independent timing/ABI oracles.

Run after scripts/enter-env.ps1 and generation. Uses MSVC and the real guest
register/memory header, then executes the actual generated main loop and state
functions against fixture callees, without game assets or a GPU.
Each case runs separately because the production clock/rate cache is static.
"""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def generated_function(name, generated_dir):
    for path in generated_dir.glob("*.c"):
        match = re.search(r"void " + name + r"\(void\)\n\{.*?\n\}",
                          path.read_text(encoding="utf-8"), re.S)
        if match:
            return match.group()
    raise AssertionError("Missing generated simulation lifecycle function: " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated-dir", type=Path, default=ROOT / "src/recomp/gen",
                        help="Read generated functions from an existing checkout without modifying it")
    args = parser.parse_args()
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-simulation-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "simulation-contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W3", "/WX", "/Od", "/std:c11",
            "/I" + str(folder),
            "/I" + str(ROOT / "tools/xboxrecomp/include"),
            str(ROOT / "tests/mm3_simulation_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)

        # Keep the real one-time initialization and endless frame-loop branches.
        # Rename only definitions of the two production-wrapped state functions.
        lifecycle = []
        for name in ("sub_00025422", "sub_0002545A", "sub_001E77F3"):
            source = generated_function(name, args.generated_dir)
            if name != "sub_001E77F3":
                source = source.replace("void " + name + "(void)",
                                        "void " + name + "_original(void)", 1)
            lifecycle.append(source)
        (folder / "simulation_mainloop_generated.h").write_text(
            "\n\n".join(lifecycle), encoding="utf-8")
        loop_executable = folder / "simulation-mainloop.exe"
        subprocess.run([
            "cl", "/nologo", "/W3", "/WX", "/wd4101", "/wd4102", "/Od", "/std:c11",
            "/I" + str(folder),
            "/I" + str(ROOT / "tools/xboxrecomp/include"),
            str(ROOT / "tests/mm3_simulation_mainloop_contract.c"),
            "/Fo:" + str(folder / "mainloop.obj"), "/Fe:" + str(loop_executable),
        ], check=True, cwd=folder)

        cases = [(mode, str(hz), hz, render_hz)
                 for hz in (30, 60)
                 for mode, render_hz in (("timing", 60), ("timing", 90),
                                         ("irregular", 60), ("irregular", 90),
                                         ("abi", 60), ("callers", 60),
                                         ("invalid", 60), ("off", 90))]
        cases += [("timing", None, 30, 60), ("timing", "75", 30, 90)]
        cases = [(*case, None) for case in cases]
        cases += [(mode, str(hz), hz, 90, drawing)
                  for hz in (30, 60) for drawing in ("0", "1")
                  for mode in ("timing", "abi", "callers", "off")]
        cases += [("timing", "30", 30, 90, "invalid")]
        for mode, configured_hz, expected_hz, render_hz, drawing in cases:
            environment = os.environ.copy()
            environment.pop("MM3_SIM_HZ", None)
            environment.pop("MM3_SIM_TRACE", None)
            environment.pop("MM3_SIM_TRACE_HARD_EXIT", None)
            environment.pop("MM3_RENDER_EVERY_FRAME", None)
            if drawing is not None:
                environment["MM3_RENDER_EVERY_FRAME"] = drawing
            if configured_hz is not None:
                environment["MM3_SIM_HZ"] = configured_hz
            environment["MM3_MOD_FRAMERATE"] = "0" if mode == "off" else "1"
            subprocess.run([str(executable), mode, str(expected_hz), str(render_hz)],
                           check=True, cwd=folder, env=environment)
        loop_cases = [(30, 0, 1, None), (60, 0, 1, None), (90, 0, 1, None),
                      (120, 0, 1, None), (240, 0, 1, None), (60, 1, 1, None),
                      (90, 1, 1, None), (90, 0, 0, None), (90, 0, 1, "0"),
                      (90, 0, 1, "1"), (90, 1, 1, "1"), (90, 0, 0, "1")]
        for hz in (30, 60):
            for render_hz, irregular, enabled, drawing in loop_cases:
                environment = os.environ.copy()
                environment["MM3_SIM_HZ"] = str(hz)
                environment["MM3_MOD_FRAMERATE"] = str(enabled)
                environment.pop("MM3_SIM_TRACE", None)
                environment.pop("MM3_SIM_TRACE_HARD_EXIT", None)
                environment.pop("MM3_RENDER_EVERY_FRAME", None)
                if drawing is not None:
                    environment["MM3_RENDER_EVERY_FRAME"] = drawing
                trace = hz == 30 and render_hz == 90 and not irregular and enabled and drawing is None
                if trace:
                    environment["MM3_SIM_TRACE"] = "1"
                    environment["MM3_SIM_TRACE_HARD_EXIT"] = "1"
                result = subprocess.run([str(loop_executable), str(hz), str(render_hz),
                                         str(irregular), str(enabled)],
                                        check=True, cwd=folder, env=environment,
                                        capture_output=trace, text=True)
                if trace:
                    print(result.stdout, end="")
                    reports = re.findall(
                        r"\[SIM_TRACE\] hz=(\d+) frames=(\d+) ticks=(\d+) "
                        r"elapsed=([\d.]+) guest=([\d.]+) world=([\d.]+) "
                        r"dropped=([\d.]+) lag=([-\d.]+)", result.stderr)
                    assert len(reports) >= 5, "Missing per-second simulation diagnostics"
                    for rate, frames, ticks, elapsed, guest, world, dropped, lag in reports:
                        assert int(rate) == hz and int(frames) > int(ticks)
                        assert abs(float(elapsed) - float(guest)) < 0.000002
                        assert abs(float(world) - int(ticks) / hz) < 0.000002
                        assert float(dropped) == 0.0
                        assert abs(float(lag)) <= 1.0 / hz + 0.000002

        # Reproduce the frozen startup scheduler's one-tick-per-render effect.
        # The actual translated loop must reject it, despite plausible fixed dt.
        current = (ROOT / "src/mods/framerate.c").read_text(encoding="utf-8")
        assert "clock += elapsed;" in current
        harness = (ROOT / "tests/mm3_simulation_mainloop_contract.c").read_text(encoding="utf-8")
        harness = harness.replace('#include "../src/mods/mods.h"',
                                  '#include "' + (ROOT / "src/mods/mods.h").as_posix() + '"')
        environment = os.environ.copy()
        environment["MM3_SIM_HZ"] = "30"
        environment.pop("MM3_SIM_TRACE", None)
        environment.pop("MM3_SIM_TRACE_HARD_EXIT", None)
        environment.pop("MM3_RENDER_EVERY_FRAME", None)
        for name, needle, replacement in (("frozen", "clock += elapsed;", "clock += TICK;"),
                                           ("duplicate-draw", "s_ticks == 0", "0")):
            assert needle in current
            (folder / (name + ".c")).write_text(current.replace(needle, replacement, 1), encoding="utf-8")
            mutant_source = folder / (name + "-mainloop.c")
            mutant_source.write_text(harness.replace('#include "../src/mods/framerate.c"',
                                                     '#include "' + name + '.c"'), encoding="utf-8")
            mutant_executable = folder / (name + "-mainloop.exe")
            subprocess.run([
                "cl", "/nologo", "/W3", "/WX", "/wd4101", "/wd4102", "/Od", "/std:c11",
                "/I" + str(folder), "/I" + str(ROOT / "src/mods"),
                "/I" + str(ROOT / "tools/xboxrecomp/include"), str(mutant_source),
                "/Fo:" + str(folder / (name + ".obj")), "/Fe:" + str(mutant_executable),
            ], check=True, cwd=folder)
            result = subprocess.run([str(mutant_executable), "30", "60", "0", "1"],
                                    cwd=folder, env=environment, capture_output=True)
            assert result.returncode != 0, "Actual loop accepted negative control: " + name
    print(f"PASS: {len(cases)} timing/ABI cases and {2 * len(loop_cases)} actual generated-loop cases; current timer drives 30/60 Hz world time through 240 frame-loop FPS; default and optional every-frame drawing preserve RET 8; independent clock trace, frozen-cadence and duplicate-draw negative controls passed")


if __name__ == "__main__":
    main()
