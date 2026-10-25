#!/usr/bin/env python3
"""Run MM3 with real emulated USB presses to shorten intro playback.

Usage:
  python tools/scripts/run_movie_skip.py [--headless] [--input-mode Live|Timed]
      [--button start|a|b] [--skip-movies dice.bik msgs.bik ...]
      [--duration-seconds 45] [--skip-delay-seconds 0]
      [--trace-post-movie-frame]
      [--executable PATH]

Requires an executable built with MM3_MOVIE_INPUT_TRACE=ON. Live mode presses
only during the selected movies (identified by the [MOVIE_FRAME] total= count)
and stops once they close. Timed mode repeats a fixed USB timeline over the
run. Each run gets its own save and logs under conformance_tmp/movie_skip_<stamp>.
--trace-post-movie-frame requests one GPU frame trace 5 s after intro.bik closes.

Default executable: build-msvc-tailfix/movie-input/RelWithDebInfo/mm3_recomp.exe
on Windows, build-msvc-tailfix/movie-input/mm3_recomp elsewhere.

Only one recomp instance may run.
"""
import argparse
import datetime
import os
import re
import subprocess
import time

MOVIE_BY_TOTAL = {144: "dice.bik", 308: "msgs.bik", 3114: "intro.bik",
                  3092: "AttractMode2.bik", 3108: "AttractMode0.bik", 3250: "AttractMode1.bik"}
MOVIE_CHOICES = ["dice.bik", "msgs.bik", "intro.bik", "AttractMode0.bik", "AttractMode1.bik", "AttractMode2.bik"]
REQUIRED_TIMED = ["dice.bik", "msgs.bik", "intro.bik"]


def die(msg):
    raise SystemExit(msg)


def game_exe(build_dir, config):
    if os.name == "nt":
        return os.path.join(build_dir, config, "mm3_recomp.exe")
    return os.path.join(build_dir, "mm3_recomp")


def game_running():
    if os.name == "nt":
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq mm3_recomp.exe", "/NH"],
                             capture_output=True, text=True).stdout
        return "mm3_recomp.exe" in out.lower()
    return subprocess.run(["pgrep", "-x", "mm3_recomp"], capture_output=True).returncode == 0


def launch_kwargs(headless):
    if headless and os.name == "nt":
        si = subprocess.STARTUPINFO()
        si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        si.wShowWindow = subprocess.SW_HIDE
        return {"startupinfo": si}
    return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--executable", default=game_exe("build-msvc-tailfix/movie-input", "RelWithDebInfo"))
    parser.add_argument("--input-mode", dest="input_mode", choices=["Live", "Timed"], default="Live")
    parser.add_argument("--button", choices=["start", "a", "b"], default="start")
    parser.add_argument("--skip-movies", dest="skip_movies", nargs="*", choices=MOVIE_CHOICES,
                        default=["dice.bik", "msgs.bik", "intro.bik"])
    parser.add_argument("--duration-seconds", dest="duration", type=int, default=45, help="1-600")
    parser.add_argument("--skip-delay-seconds", dest="skip_delay", type=int, default=0, help="0-600")
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--trace-post-movie-frame", dest="trace_post_movie_frame", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.duration <= 600:
        die("--duration-seconds must be between 1 and 600")
    if not 0 <= args.skip_delay <= 600:
        die("--skip-delay-seconds must be between 0 and 600")

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    exe = args.executable if os.path.isabs(args.executable) else os.path.join(repo, args.executable)
    if not os.path.exists(exe):
        die(f"Executable missing: {exe}")
    if game_running():
        die("Stop the existing recomp instance before launching another.")
    if args.input_mode == "Timed" and (len(args.skip_movies) != 3 or
                                       any(m not in args.skip_movies for m in REQUIRED_TIMED)):
        die("Use Live mode to target individual movies; Timed mode presses on a fixed timeline.")

    now = datetime.datetime.now()
    run = os.path.join(repo, "conformance_tmp", "movie_skip_" + now.strftime("%Y%m%d_%H%M%S") +
                       f"_{now.microsecond // 1000:03d}")
    os.makedirs(os.path.join(run, "save", "Cache"))
    live = os.path.join(run, "pad.txt")
    with open(live, "w"):
        pass
    log = os.path.join(run, "stderr.log")

    settings = {
        "RECOMP_PB_EXEC": "1", "RECOMP_USB": "1", "RECOMP_INPUT_DIAG": "1", "RECOMP_USB_STATS": "1",
        "MM3_SAVE_DIR": os.path.join(run, "save"),
        "RECOMP_PAD_PRESS": None, "RECOMP_PAD_SCRIPT": None, "RECOMP_PAD_LIVE": None,
        "RECOMP_FB_WINDOW": None if args.headless else "1",
    }
    if args.trace_post_movie_frame:
        settings["RECOMP_FRAME_TRACE"] = os.path.join(run, "frame.flag")
        settings["RECOMP_FRAME_TRACE_METHODS"] = None
    if args.input_mode == "Live":
        settings["RECOMP_PAD_LIVE"] = live
    else:
        settings["RECOMP_PAD_SCRIPT"] = ",".join(
            f"{500 + i * 500}:{args.button}:200" for i in range(0, min(255, args.duration * 2) + 1))

    env = dict(os.environ)
    for key, value in settings.items():
        if value is None:
            env.pop(key, None)
        else:
            env[key] = str(value)
    with open(os.path.join(run, "stdout.log"), "wb") as stdout, open(log, "wb") as stderr:
        process = subprocess.Popen([exe], cwd=repo, env=env, stdout=stdout, stderr=stderr,
                                   **launch_kwargs(args.headless))
    print(f"PID={process.pid} evidence={run}", flush=True)

    clock = time.monotonic()

    def elapsed_ms():
        return int((time.monotonic() - clock) * 1000)

    active = ""
    last_press = -1000
    movie_started = 0
    pending = ""
    frame_trace_at = float("inf")
    try:
        with open(log, "r", encoding="utf-8", errors="replace") as reader:
            while process.poll() is None and elapsed_ms() < args.duration * 1000:
                parts = (pending + reader.read()).split("\n")
                pending = parts[-1]
                for line in parts[:-1]:
                    match = re.search(r"\[MOVIE_FRAME\] total=(\d+) frame=(\d+)", line)
                    if match:
                        previous = active
                        active = MOVIE_BY_TOTAL.get(int(match.group(1)), "")
                        if active != previous:
                            movie_started = elapsed_ms()
                            print(f"MOVIE_WAIT name={active} delay={args.skip_delay}", flush=True)
                    if "[MOVIE_END]" in line:
                        active = ""
                        print(line, flush=True)
                        if args.trace_post_movie_frame and "name=intro.bik" in line:
                            frame_trace_at = elapsed_ms() + 5000
                    if re.search(r"\[MOVIE_SKIP_INPUT\]|\[CRASH\]", line):
                        print(line, flush=True)
                if elapsed_ms() >= frame_trace_at:
                    with open(settings["RECOMP_FRAME_TRACE"], "w") as f:
                        f.write("capture")
                    frame_trace_at = float("inf")
                if (args.input_mode == "Live" and active in args.skip_movies and
                        elapsed_ms() - movie_started >= args.skip_delay * 1000 and
                        elapsed_ms() - last_press >= 500):
                    with open(live, "a") as f:
                        f.write(f"{args.button}:200\n")
                    last_press = elapsed_ms()
                time.sleep(0.1)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    # Timed mode is deliberately a fixed repeating timeline; use Live mode to
    # target selected movies. Both paths generate USB reports, never desktop keys.


if __name__ == "__main__":
    main()
