"""Exercise shared race guidance against the native marker projection/ABI."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from mm3_map_zoom_regression import constants

ROOT = Path(__file__).resolve().parents[1]
OWNERS = ("sub_00201BCA", "sub_0011CAE8", "sub_0020C9F9", "sub_00120091")
MATH = ("sub_0002DC95", "sub_0006ACC3", "sub_000C6ADC", "sub_0006AC61",
        "sub_0006AE0C", "sub_0006AEE2", "sub_0001259E", "sub_0006AD9F", "sub_0006AE60")
HOOKS = {
    OWNERS[0]: ("loc_00201BCA", "loc_00201CFA", "loc_00201D23", "loc_00201D55"),
    OWNERS[1]: ("loc_0011CB45", "loc_0011CC53", "loc_0011CF36"),
    OWNERS[2]: ("loc_0020CD59",),
    OWNERS[3]: ("loc_00120091",),
}


def main():
    if not shutil.which("cl") or not shutil.which("cmake"):
        raise SystemExit("Run scripts/enter-env.ps1 first.")
    native = {}
    for path in sorted((ROOT / "src/recomp/gen").glob("recomp_*.c")):
        source = path.read_text()
        for name in OWNERS + MATH:
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in native, name
                native[name] = match.group()
    assert set(native) == set(OWNERS + MATH)
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert "list(APPEND MM3_SEAM_FUNCTIONS " + " ".join(OWNERS) + ")" in cmake
    assert "mm3_add_race_guidance(distance_content ${race_owner})" in cmake
    assert "src/mm3_race_guidance.c" in cmake
    with tempfile.TemporaryDirectory(prefix="mm3-race-guidance-") as temporary:
        folder = Path(temporary)
        original, output, script = folder / "original.c", folder / "output.c", folder / "seam.cmake"
        script.write_text(
            f'include("{(ROOT / "scripts/mm3_race_guidance_seam.cmake").as_posix()}")\n'
            'file(READ "${INPUT}" content)\n'
            'mm3_add_race_guidance(content "${OWNER}")\n'
            'file(WRITE "${OUTPUT}" "${content}")\n')
        def transform(source, name, reject=False):
            original.write_bytes(source.encode())
            result = subprocess.run(["cmake", f"-DINPUT={original}", f"-DOUTPUT={output}",
                                     f"-DOWNER={name}", "-P", str(script)],
                                    capture_output=True, text=True)
            if reject:
                assert result.returncode and "MM3 race guidance" in result.stderr, result
                return
            assert not result.returncode, result.stderr
            return output.read_text()
        modified = {}
        for name in OWNERS:
            modified[name] = transform(native[name], name)
            assert transform(native[name].replace("\n", "\r\n"), name) == modified[name]
            # Keep every original native instruction, in the same order.
            cursor = modified[name].index("void " + name)
            for line in native[name].splitlines():
                cursor = modified[name].index(line + "\n", cursor) + len(line) + 1 if line != "}" else cursor
            for label in HOOKS[name]:
                transform(native[name].replace(label + ": ;", label + "BAD: ;"), name, True)
            transform(native[name] + "\n" + native[name], name, True)
            transform(modified[name], name, True)
            unrelated = "\nvoid unrelated(void)\n{\n" + HOOKS[name][0] + ": ;\n}\n"
            assert transform(native[name] + unrelated, name).endswith(unrelated)
        declarations = "\n".join("void " + name + "(void);" for name in MATH)
        body = "\n".join(native[name] for name in MATH)
        marker = OWNERS[1]
        body += "\n" + native[marker].replace("void " + marker, "void retail_marker", 1)
        body += "\n" + modified[marker]
        body += "\n" + modified[OWNERS[2]]
        body += "\n" + modified[OWNERS[3]]
        (folder / "native_race_guidance.inc").write_text(declarations + "\n" + body + constants(body))
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "src/mm3_race_guidance.c", folder / "production_race_guidance.c")
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_race_guidance_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)
    print("PASS race guidance seams: source owners, LF/CRLF, unchanged native instructions and invalid anchors")


if __name__ == "__main__":
    main()
