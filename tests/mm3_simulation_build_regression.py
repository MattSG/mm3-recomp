"""Verify actual configured simulation call targets after C preprocessing.

Run after configuring the production Ninja build and scripts/enter-env.ps1.
This checks the generated main-loop translation unit with its actual hook
definitions, then proves a whole-chunk rename would bypass the world wrapper.
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HOOKS = ("sub_00025422", "sub_0002545A", "sub_001E7590", "sub_001EC8E6")


def body(source, name):
    match = re.search(r"\bvoid\s+" + name + r"\s*\(\s*void\s*\)\s*{", source)
    if not match:
        raise AssertionError(f"Missing generated definition: {name}")
    cursor, depth = match.end(), 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[match.end():cursor - 1]


def records(folder, config):
    path = folder / "CMakeFiles" / f"impl-{config}.ninja"
    if not path.is_file():
        path = folder / "build.ninja"
    text = path.read_text(encoding="utf-8")
    for block in re.split(r"\n(?=build )", text):
        head = block.partition("\n")[0]
        match = re.search(r": C_COMPILER_\S+ (.*?) \|\|", head)
        if not match:
            continue
        filename = match.group(1).replace("$:", ":").replace("$ ", " ").replace("$$", "$")
        defines = re.search(r"^  DEFINES = (.*)$", block, re.M)
        yield Path(filename), re.findall(r"-D(sub_[0-9A-F]+=[^\s]+)", defines.group(1) if defines else "")


def preprocess(source, defines, generated_dir):
    command = ["cl", "/nologo", "/EP", "/TC", "/I" + str(generated_dir),
               "/I" + str(ROOT / "tools/xboxrecomp/include")]
    command += ["/D" + value for value in defines]
    result = subprocess.run(command + [str(source)], capture_output=True, text=True, check=True)
    return result.stdout


def check_calls(main):
    for hook in HOOKS:
        assert re.search(r"\b" + hook + r"\b", main), f"Main loop bypasses {hook}"
        assert not re.search(r"\b" + hook + r"_original\b", main), f"Main loop calls original {hook}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--generated-dir", type=Path,
                        help="Override generated headers; defaults to the configured RECOMP_GEN_DIR")
    args = parser.parse_args()
    args.build_dir = args.build_dir.resolve()
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    entries = list(records(args.build_dir, args.config))
    generated_dir = args.generated_dir.resolve() if args.generated_dir is not None else None
    if generated_dir is None:
        cache = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
        match = re.search(r"^RECOMP_GEN_DIR:PATH=(.+)$", cache, re.M)
        assert match, "Missing configured generated directory"
        generated_dir = Path(match.group(1)).resolve()
    definitions = {}
    loop = None
    for source, defines in entries:
        if source.parent not in (generated_dir, args.build_dir / "render_distance",
                                 args.build_dir / "hooked_chunks"):
            continue
        text = source.read_text(encoding="utf-8")
        for hook in HOOKS:
            if re.search(r"\bvoid\s+" + hook + r"_original\s*\(void\)", text):
                assert hook not in definitions, f"Duplicate original {hook}"
                assert not any(value.startswith(hook + "=") for value in defines), f"Whole-chunk rename for {hook}"
                definitions[hook] = source
        if re.search(r"\bvoid\s+sub_001E77F3\s*\(void\)", text):
            assert loop is None, "Duplicate main loop"
            loop = source, defines
    assert set(definitions) == set(HOOKS), "Configured build must rename all simulation definitions only"
    assert loop is not None
    source, defines = loop
    expanded = preprocess(source, defines, generated_dir)
    check_calls(body(expanded, "sub_001E77F3"))
    assert re.search(r"\bvoid\s+sub_001E7590_original\s*\(void\)\s*{", expanded)
    for hook in ("sub_001E7590", "sub_001EC8E6"):
        mutant = preprocess(source, defines + [hook + "=" + hook + "_original"], generated_dir)
        try:
            check_calls(body(mutant, "sub_001E77F3"))
        except AssertionError:
            pass
        else:
            raise AssertionError("Whole-chunk hook rename escaped the negative control: " + hook)
    print("PASS: configured/preprocessed main loop calls all simulation wrappers; original definitions stay separate; whole-chunk rename rejected")


if __name__ == "__main__":
    main()
