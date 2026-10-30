"""Exercise actual launcher binding exports through the native USB input path."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def declaration(source, pattern):
    match = re.search(pattern + r".*?^};", source, re.S | re.M)
    if not match:
        raise AssertionError(f"Launcher declaration missing: {pattern}")
    return match.group(0)


def function(source, name):
    match = re.search(r"^[^\n;]*\b" + name + r"\([^\n]*\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(f"Launcher function missing: {name}")
    depth = 1
    cursor = match.end()
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    return source[match.start():cursor]


def main():
    cc.skip_unless_windows("the fixtures include windows.h and link user32")
    cc.require_compiler()
    source = (ROOT / "src/mm3_launcher.cpp").read_text(encoding="utf-8")
    parts = [
        "#include <windows.h>\n#include <stdio.h>\n#include <stdlib.h>\n"
        "#include <string.h>\n#include <ctype.h>\n#include <string>\n#include <vector>\n"
        "namespace Rml { using String = std::string; template<class T> using Vector = std::vector<T>; }\n"
        "#include \"keymap_spec.h\"\n"
        "#include \"" + (ROOT / "tools/xboxrecomp/src/input/keymap_spec.c").as_posix() + "\"\n",
        declaration(source, r"struct Bind\s*{"),
        declaration(source, r"Rml::Vector<Bind> g_binds\s*="),
    ]
    for name in ("kPadSources", "kPadSourceNames"):
        match = re.search(r"const char \*const " + name + r"\[16\].*?;", source, re.S)
        if not match:
            raise AssertionError(f"Launcher array missing: {name}")
        parts.append(match.group(0))
    for name in ("key_name", "pad_bind", "set_key", "parse_keymap", "keymap"):
        parts.append(function(source, name))
    parts.append(r'''
int main(int argc, char **argv) {
    for (Bind &b : g_binds) set_key(b, b.def);
    if (argc > 1) {
        /* These are saved INI values, using the same loader as the launcher. */
        parse_keymap("RECOMP_KEYMAP", "RT:57");
        parse_keymap("RECOMP_PADMAP", "BLACK:LB,WHITE:RB");
    }
    Rml::String keyboard = keymap("RECOMP_KEYMAP");
    Rml::String controller = keymap("RECOMP_PADMAP");
    for (Bind &b : g_binds) set_key(b, b.def);
    parse_keymap("RECOMP_KEYMAP", keyboard.c_str());
    parse_keymap("RECOMP_PADMAP", controller.c_str());
    /* Save/reload must retain all choices, including custom shoulders. */
    if (keymap("RECOMP_KEYMAP") != keyboard || keymap("RECOMP_PADMAP") != controller) return 1;
    parse_keymap("RECOMP_KEYMAP", "WRONGRT:32");
    parse_keymap("RECOMP_PADMAP", "WRONGBLACK:A");
    if (keymap("RECOMP_KEYMAP") != keyboard || keymap("RECOMP_PADMAP") != controller) return 1;
    printf("RECOMP_KEYMAP=%s\nRECOMP_PADMAP=%s\n", keyboard.c_str(), controller.c_str());
    return 0;
}
''')
    with tempfile.TemporaryDirectory(prefix="mm3-launcher-controls-") as temporary:
        folder = Path(temporary)
        utility = folder / "launcher_maps.cpp"
        utility.write_text("\n".join(parts), encoding="utf-8")
        maps_exe = folder / f"launcher_maps{cc.exe_suffix()}"
        input_exe = folder / f"input_contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [utility], maps_exe, std="c++17", optimize="speed", warnings="normal",
            defines=["_CRT_SECURE_NO_WARNINGS"], includes=[ROOT / "tools/xboxrecomp/src/input"],
            libs=["user32"], obj=folder / "launcher_maps.obj"),
            cwd=folder, check=True)
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_input_contract.c"] + [ROOT / "tools/xboxrecomp/src/input" / f for f in
             ("mkb_input.c", "keyboard.c", "keymap_spec.c", "input_map.c")], input_exe, std="c11", optimize="speed",
            warnings="normal", defines=["WIN32_LEAN_AND_MEAN", "NOMINMAX", "_CRT_SECURE_NO_WARNINGS"],
            includes=[ROOT / "tools/xboxrecomp/src"], libs=["user32"]), cwd=folder, check=True)
        for saved in (False, True):
            command = [str(maps_exe)] + (["saved"] if saved else [])
            exported = subprocess.check_output(command, text=True, cwd=folder)
            env = os.environ.copy()
            env["RECOMP_MENU_KEYS"] = ""
            env.update(line.split("=", 1) for line in exported.splitlines())
            args = [str(input_exe), "mm3"] + (["saved-maps"] if saved else [])
            subprocess.run(args, env=env, cwd=folder, check=True)
    print("Launcher defaults and saved mappings preserve native controls and MM3 driving aliases.")


if __name__ == "__main__":
    main()
