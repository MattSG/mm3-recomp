#!/usr/bin/env python3
"""Run the clean build with diagnostics and recovery toggles cleared.

Usage:
  python tools/scripts/run_clean_strict.py --run-id NAME [--seconds 900]

Launches build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe (Windows) or
build-msvc-tailfix/mm3_recomp (elsewhere) with every MM3_*, RECOMP_* and NV2A_*
variable removed except MM3_SAVE_DIR, RECOMP_KEYBOARD and RECOMP_FB_WINDOW,
then sets MM3_RUN_ID. Evidence goes to conformance_tmp/clean-strict/<run-id>/
(stdout.log, stderr.log, run.json). Exits 1 on timeout, crash or nonzero exit.

The child gets its environment
as a dict instead of by mutating this process, so nothing needs restoring.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys


def die(msg):
    raise SystemExit(msg)


def game_exe(build_dir, config):
    if os.name == "nt":
        return os.path.join(build_dir, config, "mm3_recomp.exe")
    return os.path.join(build_dir, "mm3_recomp")


def git_short_head(cwd):
    return subprocess.check_output(["git", "-C", cwd, "rev-parse", "--short", "HEAD"], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--run-id", required=True, help="letters, digits, '_' and '-' only")
    parser.add_argument("--seconds", type=int, default=900, help="timeout, 30-3600 (default 900)")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.run_id):
        die("--run-id may contain only letters, digits, '_' and '-'")
    if not 30 <= args.seconds <= 3600:
        die("--seconds must be between 30 and 3600")

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    exe = game_exe(os.path.join(repo, "build-msvc-tailfix"), "RelWithDebInfo")
    if not os.path.exists(exe):
        die(f"Missing clean build: {exe}")

    run_dir = os.path.join(repo, "conformance_tmp", "clean-strict", args.run_id)
    if os.path.exists(run_dir):
        die(f"Run directory already exists: {run_dir}")
    os.makedirs(run_dir)
    out = os.path.join(run_dir, "stdout.log")
    err = os.path.join(run_dir, "stderr.log")
    meta = os.path.join(run_dir, "run.json")
    with open(exe, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest().upper()
    outer = git_short_head(repo)
    toolkit = git_short_head(os.path.join(repo, "tools", "xboxrecomp"))

    keep = {"MM3_SAVE_DIR", "RECOMP_KEYBOARD", "RECOMP_FB_WINDOW"}
    cleared = sorted(name for name in os.environ
                     if re.match(r"^(MM3_|RECOMP_|NV2A_)", name) and name not in keep)
    child_env = {k: v for k, v in os.environ.items() if k not in cleared}
    child_env["MM3_RUN_ID"] = args.run_id

    timed_out = False
    with open(out, "wb") as stdout, open(err, "wb") as stderr:
        process = subprocess.Popen([exe], cwd=repo, env=child_env, stdout=stdout, stderr=stderr)
        try:
            exit_code = process.wait(timeout=args.seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            exit_code = process.wait()

    with open(err, encoding="utf-8", errors="replace") as f:
        crash = bool(re.search(r"\[CRASH\]|Access violation|0xC0000005", f.read()))
    if timed_out:
        status = "timeout"
    elif crash:
        status = "crash"
    elif exit_code != 0:
        status = "failed"
    else:
        status = "process_exit_only"

    kb = os.environ.get("RECOMP_KEYBOARD")
    fb = os.environ.get("RECOMP_FB_WINDOW")
    result = {
        "run_id": args.run_id,
        "status": status,
        "timeout_seconds": args.seconds,
        "timed_out": timed_out,
        "exit_code": exit_code,
        "executable": exe,
        "sha256": digest,
        "outer_commit": outer,
        "toolkit_commit": toolkit,
        "keyboard_input_enabled": bool(kb) and kb != "0",
        "framebuffer_window_enabled": bool(fb) and fb != "0",
        "diagnostics_and_recovery_env_cleared": cleared,
        "save_dir_preserved": bool(os.environ.get("MM3_SAVE_DIR")),
        "stdout": out,
        "stderr": err,
        "visual_acceptance": "not measured by this runner",
    }
    with open(meta, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(f"[{args.run_id}] status={status} exit={exit_code} exe={exe} sha256={digest}")
    print(f"[{args.run_id}] stdout={out} stderr={err} metadata={meta}")
    if exit_code != 0 or timed_out or crash:
        sys.exit(1)


if __name__ == "__main__":
    main()
