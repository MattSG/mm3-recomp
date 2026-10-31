"""Where does the game thread spend its time? Samples it for a while in a live Checkpoint race.

Usage: MM3_SAVE_TEMPLATE=<copied save> python tools/game_test/profile_thread.py [seconds] [fps_limit]
Uses the debug pipe's `profile` command (src/mods/teleport.c): the thread is sampled every
millisecond. The share inside the executable's translated code, in ntdll (blocked in a wait) and
elsewhere is printed, with the hottest exe code and the exe functions the thread is blocked under. An fps_limit of 0 (the default here) leaves the frame cap off.
The exe rvas can be symbolised with dbghelp against the build's pdb.
"""
import collections, os, sys, tempfile, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.mission_win import ALL_MODS, CHECKPOINT
from tools.game_test.run import Session


def symbolize(exe, rvas):
    """Function names for exe rvas, through dbghelp and the pdb beside the exe (Windows)."""
    import ctypes
    from ctypes import wintypes as w
    dh = ctypes.WinDLL("dbghelp.dll")
    dh.SymInitialize.argtypes = [w.HANDLE, w.LPCSTR, w.BOOL]
    dh.SymLoadModuleEx.argtypes = [w.HANDLE, w.HANDLE, w.LPCSTR, w.LPCSTR, ctypes.c_uint64, w.DWORD, ctypes.c_void_p, w.DWORD]
    dh.SymLoadModuleEx.restype = ctypes.c_uint64
    dh.SymFromAddr.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.c_void_p]
    dh.SymSetOptions(0x2 | 0x4 | 0x10000000)
    proc = ctypes.windll.kernel32.GetCurrentProcess()
    base = 0x140000000

    class Sym(ctypes.Structure):
        _fields_ = [("SizeOfStruct", w.ULONG), ("TypeIndex", w.ULONG), ("Reserved", ctypes.c_uint64 * 2), ("Index", w.ULONG),
                    ("Size", w.ULONG), ("ModBase", ctypes.c_uint64), ("Flags", w.ULONG), ("Value", ctypes.c_uint64),
                    ("Address", ctypes.c_uint64), ("Register", w.ULONG), ("Scope", w.ULONG), ("Tag", w.ULONG),
                    ("NameLen", w.ULONG), ("MaxNameLen", w.ULONG), ("Name", ctypes.c_char * 512)]
    dh.SymInitialize(proc, None, False)
    dh.SymLoadModuleEx(proc, None, str(exe).encode(), None, base, 0, None, 0)
    names = {}
    for r in rvas:
        sym, disp = Sym(), ctypes.c_uint64(0)
        sym.SizeOfStruct, sym.MaxNameLen = 88, 511
        ok = dh.SymFromAddr(proc, base + int(r, 16), ctypes.byref(disp), ctypes.byref(sym))
        names[r] = "%s+%#x" % (sym.Name.decode(), disp.value) if ok else "?"
    return names


def process_cpu_seconds(pid):
    """Kernel plus user CPU time of a process, in seconds (Windows)."""
    import ctypes
    from ctypes import wintypes as w
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = w.HANDLE
    handle = k.OpenProcess(0x1000, False, pid)           # PROCESS_QUERY_LIMITED_INFORMATION
    times = [w.FILETIME() for _ in range(4)]
    ok = k.GetProcessTimes(handle, *[ctypes.byref(t) for t in times])
    k.CloseHandle(handle)
    if not ok:
        return None
    ticks = lambda t: (t.dwHighDateTime << 32 | t.dwLowDateTime)
    return (ticks(times[2]) + ticks(times[3])) / 1e7


def summarise(path, exe=None):
    lines = [l.split() for l in open(path) if l.strip()]
    shares = collections.Counter(l[0] for l in lines)
    total = len(lines)
    print("samples:", total)
    for name, n in shares.most_common(6):
        print("  %-20s %5.1f%%" % (name, 100.0 * n / total))
    hot = collections.Counter(l[1] for l in lines if l[0] == "exe")
    waits = collections.Counter(l[2].split("=")[1] for l in lines if l[0] != "exe" and len(l) > 2)
    pick = [r for r, _ in hot.most_common(5)] + [r for r, _ in waits.most_common(8)]
    names = symbolize(exe, set(pick)) if exe and sys.platform == "win32" else {}
    print("hottest code in the exe:")
    for r, n in hot.most_common(5):
        print("  %5.1f%%  %s  %s" % (100.0 * n / total, r, names.get(r, "")))
    print("what the thread is blocked in (first exe frame of its stack):")
    for r, n in waits.most_common(8):
        print("  %5.1f%%  %s  %s" % (100.0 * n / total, r, names.get(r, "(no exe frame)" if r == "0" else "")))


def main(seconds=20, fps_limit="0"):
    out = os.path.join(tempfile.gettempdir(), "mm3-profile.txt").replace("\\", "/")
    env = dict(ALL_MODS, RECOMP_FPS_LIMIT=str(fps_limit))
    exe = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "build/RelWithDebInfo/mm3_recomp.exe"))
    with Session(exe,
                 os.path.join(os.path.dirname(__file__), "..", "..", "game_files"),
                 save_template=os.environ["MM3_SAVE_TEMPLATE"], timeout=seconds + 240, env=env) as g:
        g.run({"steps": CHECKPOINT})
        g.press("rt", 60000, wait_release=False)
        time.sleep(12)                               # past the countdown
        pid = g.process.pid
        cpu0, wall0 = process_cpu_seconds(pid), time.time()
        print(g.request("profile %d %s" % (seconds, out)), flush=True)
        time.sleep(seconds)
        cpu1, wall1 = process_cpu_seconds(pid), time.time()
        if cpu0 is not None and cpu1 is not None:
            print("game process CPU while sampled: %.2f cores" % ((cpu1 - cpu0) / (wall1 - wall0)))
        time.sleep(4)
    summarise(out, exe)


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 20, sys.argv[2] if len(sys.argv) > 2 else "0")
