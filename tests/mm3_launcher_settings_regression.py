"""Check production launcher profile/INI precedence without a rendered UI.

Run after scripts/enter-env.ps1. The temporary MSVC fixture compiles the actual
Option/Choice/Bind tables, mod registry and settings helpers. Only RmlUi's
string/vector types are substituted with their standard-library equivalents.
Each case has an isolated environment and mm3_settings.ini directory.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile


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
    # Ignore strings and comments when finding the full, unchanged function.
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    depth = 1
    for token in tokens.finditer(source, match.end()):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return source[match.start():token.end()]
    raise AssertionError(f"Unterminated launcher function: {name}")


def fixture_source():
    source = (ROOT / "src/mm3_launcher.cpp").read_text(encoding="utf-8")
    parts = [
        "#include <windows.h>\n#include <stdio.h>\n#include <stdlib.h>\n"
        "#include <string.h>\n#include <ctype.h>\n#include <string>\n#include <vector>\n"
        '#include "mods.h"\n'
        "namespace Rml { using String = std::string; template<class T> using Vector = std::vector<T>; }\n",
    ]
    for name in ("Choice", "Option", "Bind"):
        parts.append(declaration(source, r"struct " + name + r"\s*{"))
    for kind, name in (("Option", "g_options"), ("Bind", "g_binds")):
        parts.append(declaration(source, r"Rml::Vector<" + kind + "> " + name + r"\s*="))
    for name in ("kPadSources", "kPadSourceNames"):
        match = re.search(r"const char \*const " + name + r"\[16\].*?;", source, re.S)
        if not match:
            raise AssertionError(f"Launcher array missing: {name}")
        parts.append(match.group(0))
    match = re.search(r"const char \*const kSettingsFile\s*=.*?;", source)
    if not match:
        raise AssertionError("Launcher settings filename missing")
    parts.append(match.group(0))
    for name in ("key_name", "pad_bind", "set_key", "parse_keymap", "reset_all", "valid",
                 "add_mod_options", "load_environment_settings", "load_settings"):
        parts.append(function(source, name))
    parts.append(r'''
static int checks;
static void require(bool success, const char *description) {
    ++checks;
    if (!success) {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(1);
    }
}
static const Option &option(const char *env) {
    for (const Option &o : g_options) if (o.env == env) return o;
    fprintf(stderr, "FAIL: missing option %s\n", env);
    exit(1);
}
static const Bind &binding(const char *env, const char *name) {
    for (const Bind &b : g_binds) if (b.env == env && b.name == name) return b;
    fprintf(stderr, "FAIL: missing binding %s/%s\n", env, name);
    exit(1);
}
int main(int argc, char **argv) {
    const size_t original_options = g_options.size();
    load_settings();
    require(g_options.size() == original_options + MM3_MOD_COUNT - 1,
            "all user-facing mods added once, translation switch excluded");
    for (int i = 0; i < MM3_MOD_COUNT; ++i) {
        if (!mm3_mods[i].required) continue;
        const Option &o = option(mm3_mods[i].env);
        require(o.fixed && o.value == "1", "required mod remains fixed and enabled");
    }
    const Option &peers = option("RECOMP_NET_PEERS");
    require(peers.text && !peers.fixed, "text fields accept inherited/saved text");
    require(valid(peers, std::string(199, 'x')), "199-character text accepted");
    require(!valid(peers, std::string(200, 'x')), "200-character text rejected");
    require(!valid(peers, "host\nother") && !valid(peers, "host\rother"),
            "multiline text rejected");
    if (argc > 1 && !strcmp(argv[1], "reset")) {
        reset_all();
        for (const Option &o : g_options) require(o.value == o.def, "reset restores every option default");
        for (const Bind &b : g_binds) require(b.vk == b.def, "reset restores every binding default");
    }
    FILE *expected = fopen("expected.txt", "r");
    require(expected != nullptr, "independent expected values supplied");
    char line[512];
    while (fgets(line, sizeof line, expected)) {
        line[strcspn(line, "\r\n")] = 0;
        char *first = strchr(line, '|');
        require(first != nullptr, "expected record has type");
        *first++ = 0;
        char *second = strchr(first, '|');
        require(second != nullptr, "expected record has identifier");
        *second++ = 0;
        if (!strcmp(line, "option")) {
            const Option &o = option(first);
            if (o.value != second)
                fprintf(stderr, "%s expected '%s', received '%s'\n", first, second, o.value.c_str());
            require(o.value == second, "option matches independent expected value");
        } else {
            char *third = strchr(second, '|');
            require(third != nullptr, "binding expectation has source");
            *third++ = 0;
            const Bind &b = binding(first, second);
            if (b.vk != atoi(third))
                fprintf(stderr, "%s/%s expected %s, received %d\n", first, second, third, b.vk);
            require(b.vk == atoi(third), "binding matches independent expected source");
        }
    }
    fclose(expected);
    printf("PASS: %d launcher settings checks\n", checks);
    return 0;
}
''')
    mods = (ROOT / "src/mods/mods.c").read_text(encoding="utf-8")
    registry = '#include "mods.h"\n' + declaration(mods, r"const Mm3Mod mm3_mods\[MM3_MOD_COUNT\]\s*=")
    return "\n".join(parts), registry


def cases():
    # Expected values describe profile/INI behavior, independently of the loader.
    return [
        ("low-power", {"RECOMP_FPS_LIMIT": "30", "RECOMP_MSAA": "1", "RECOMP_ANISO": "4",
                        "RECOMP_DRAW_DISTANCE": "1", "RECOMP_VIEW_DISTANCE": "1", "RECOMP_RENDER_SCALE": "1.5"},
         None, {"RECOMP_FPS_LIMIT": "30", "RECOMP_MSAA": "1", "RECOMP_ANISO": "4",
                "RECOMP_DRAW_DISTANCE": "1", "RECOMP_VIEW_DISTANCE": "1", "RECOMP_RENDER_SCALE": "1.5"}, [], False),
        ("balanced", {"RECOMP_FPS_LIMIT": "60", "RECOMP_RENDER_SCALE": "2.0", "RECOMP_MSAA": "4",
                       "RECOMP_DRAW_DISTANCE": "4", "RECOMP_VIEW_DISTANCE": "2"},
         None, {"RECOMP_FPS_LIMIT": "60", "RECOMP_RENDER_SCALE": "2", "RECOMP_MSAA": "4",
                "RECOMP_DRAW_DISTANCE": "4", "RECOMP_VIEW_DISTANCE": "2"}, [], False),
        ("saved-auto", {"RECOMP_RENDER_SCALE": "2.0", "RECOMP_ASPECT": "16:10", "MM3_FOV": "120",
                         "RECOMP_WINDOW_SIZE": "1920x1080", "RECOMP_FULLSCREEN": "1", "RECOMP_FPS_LIMIT": "30"},
         "RECOMP_RENDER_SCALE=\nRECOMP_ASPECT=\nMM3_FOV=\nRECOMP_WINDOW_SIZE=\nRECOMP_FULLSCREEN=\nRECOMP_FPS_LIMIT=90\n",
         {"RECOMP_RENDER_SCALE": "", "RECOMP_ASPECT": "", "MM3_FOV": "", "RECOMP_WINDOW_SIZE": "",
          "RECOMP_FULLSCREEN": "", "RECOMP_FPS_LIMIT": "90"}, [], False),
        ("invalid-environment", {"RECOMP_FPS_LIMIT": "240", "RECOMP_RENDER_SCALE": "nan", "RECOMP_MSAA": "3",
                                 "RECOMP_ASPECT": "wide", "RECOMP_NET_PEERS": "host\nother"},
         None, {"RECOMP_FPS_LIMIT": "60", "RECOMP_RENDER_SCALE": "", "RECOMP_MSAA": "4",
                "RECOMP_ASPECT": "", "RECOMP_NET_PEERS": ""}, [], False),
        ("invalid-saved", {"RECOMP_FPS_LIMIT": "30", "RECOMP_RENDER_SCALE": "1.5", "RECOMP_MSAA": "1"},
         "RECOMP_FPS_LIMIT=240\nRECOMP_RENDER_SCALE=2.0\nRECOMP_MSAA=3\nUNKNOWN_SETTING=1\nmalformed line\n",
         {"RECOMP_FPS_LIMIT": "30", "RECOMP_RENDER_SCALE": "1.5", "RECOMP_MSAA": "1"}, [], False),
        ("maps-inherited", {"RECOMP_KEYMAP": "A:51,RT:57", "RECOMP_PADMAP": "BLACK:LB,WHITE:RB",
                            "MM3_DEV_HOTKEYS": "HUD:71,MENU:72"},
         None, {}, [("RECOMP_KEYMAP", "A", 0x51), ("RECOMP_KEYMAP", "RT", 0x57),
                    ("RECOMP_PADMAP", "BLACK", 5), ("RECOMP_PADMAP", "WHITE", 6),
                    ("MM3_DEV_HOTKEYS", "HUD", 0x71), ("MM3_DEV_HOTKEYS", "MENU", 0x72)], False),
        ("maps-saved", {"RECOMP_KEYMAP": "A:51,RT:57", "RECOMP_PADMAP": "BLACK:LB,WHITE:RB",
                        "MM3_DEV_HOTKEYS": "HUD:71,MENU:72"},
         "RECOMP_KEYMAP=A:41\nRECOMP_PADMAP=BLACK:RB,WHITE:LB\nMM3_DEV_HOTKEYS=HUD:73\n",
         {}, [("RECOMP_KEYMAP", "A", 0x41), ("RECOMP_KEYMAP", "RT", 0x57),
              ("RECOMP_PADMAP", "BLACK", 6), ("RECOMP_PADMAP", "WHITE", 5),
              ("MM3_DEV_HOTKEYS", "HUD", 0x73), ("MM3_DEV_HOTKEYS", "MENU", 0x72)], False),
        ("fixed-mods-and-text", {"MM3_MOD_COLLISION_FIX": "0", "MM3_MOD_DISC_CHECK": "0", "MM3_MOD_SKY": "0",
                                "RECOMP_NET_PEERS": "192.0.2.1:3075", "RECOMP_NET_PORT": "4000"},
         "MM3_MOD_COLLISION_FIX=0\nMM3_MOD_DISC_CHECK=0\nRECOMP_NET_PORT=5000\n",
         {"MM3_MOD_COLLISION_FIX": "1", "MM3_MOD_DISC_CHECK": "1", "MM3_MOD_SKY": "0",
          "RECOMP_NET_PEERS": "192.0.2.1:3075", "RECOMP_NET_PORT": "5000"}, [], False),
        ("reset", {"RECOMP_FPS_LIMIT": "30", "RECOMP_RENDER_SCALE": "1.5", "RECOMP_KEYMAP": "A:51",
                   "RECOMP_PADMAP": "BLACK:LB", "MM3_DEV_HOTKEYS": "HUD:71"},
         "RECOMP_FPS_LIMIT=90\nRECOMP_RENDER_SCALE=3\nRECOMP_KEYMAP=A:41\n",
         {"RECOMP_FPS_LIMIT": "60", "RECOMP_RENDER_SCALE": "", "MM3_MOD_COLLISION_FIX": "1"}, [], True),
    ]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    native, registry = fixture_source()
    with tempfile.TemporaryDirectory(prefix="mm3-launcher-settings-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mods/mods.h", folder / "mods.h")
        (folder / "settings.cpp").write_text(native, encoding="utf-8")
        (folder / "registry.c").write_text(registry, encoding="utf-8")
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/Od", "/std:c11", "/c", "registry.c",
                        "/Fo:registry.obj"], check=True, cwd=folder)
        executable = folder / "settings-contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/Od", "/std:c++17", "/D_CRT_SECURE_NO_WARNINGS",
                        "settings.cpp", "registry.obj", "user32.lib", "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        environment = {key: value for key, value in os.environ.items()
                       if not key.upper().startswith(("RECOMP_", "MM3_"))}
        for name, overrides, saved, options, bindings, reset in cases():
            case = folder / name
            case.mkdir()
            if saved is not None:
                (case / "mm3_settings.ini").write_text(saved, encoding="utf-8")
            expected = [f"option|{key}|{value}" for key, value in options.items()]
            expected += [f"binding|{env}|{key}|{value}" for env, key, value in bindings]
            (case / "expected.txt").write_text("\n".join(expected) + "\n", encoding="utf-8")
            command = [str(executable)] + (["reset"] if reset else [])
            result = subprocess.run(command,
                                    cwd=case, env=environment | overrides,
                                    capture_output=True, text=True)
            if result.returncode:
                print(result.stdout, end="")
                print(result.stderr, end="")
                raise subprocess.CalledProcessError(result.returncode, command)
            print(f"{name}: {result.stdout.strip()}")
    print("PASS: 9 isolated production launcher settings cases; profiles, numeric scale, saved Auto/invalid values, map precedence, fixed mods and reset")


if __name__ == "__main__":
    main()
