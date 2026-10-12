"""Execute the camera Arrow seam and prove minimap/world owners stay native."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OWNER = "sub_001EC352"


def native_owner(name):
    owners = []
    for path in (ROOT / "src/recomp/gen").glob("recomp_*.c"):
        match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}",
                          path.read_text(), re.M | re.S)
        if match:
            owners.append(match.group())
    assert len(owners) == 1
    return owners[0]


def main():
    if not shutil.which("cl") or not shutil.which("cmake"):
        raise SystemExit("Run scripts/enter-env.ps1 first.")
    original = native_owner(OWNER)
    minimap = native_owner("sub_0011D808")
    world = native_owner("sub_0020C9F9")
    with tempfile.TemporaryDirectory(prefix="mm3-hud-cleanup-") as temporary:
        folder = Path(temporary)
        source, output, script = (folder / name for name in
                                  ("original.c", "modified.c", "transform.cmake"))
        script.write_text(
            f'include("{(ROOT / "scripts/mm3_hud_cleanup_seam.cmake").as_posix()}")\n'
            'file(READ "${INPUT}" content)\n'
            'mm3_hide_navigation_arrows(content)\n'
            'file(WRITE "${OUTPUT}" "${content}")\n')

        def transform(text, reject=False):
            source.write_bytes(text.encode())
            result = subprocess.run(["cmake", f"-DINPUT={source}", f"-DOUTPUT={output}",
                                     "-P", str(script)], capture_output=True, text=True)
            if reject:
                assert result.returncode and "MM3 HUD cleanup" in result.stderr
                return
            assert not result.returncode, result.stderr
            return output.read_text()

        modified = transform(original)
        assert transform(original.replace("\n", "\r\n")) == modified
        restored = modified.replace("    goto loc_001EC3AC; /* Top navigation arrow hidden. */",
                                    "    if (CMP_EQ(_fa, _fb)) goto loc_001EC3AC; /* je: equal / zero */")
        assert restored == original
        assert modified.count("/* Top navigation arrow hidden. */") == 1
        unrelated = "\n" + minimap + "\n" + world
        assert transform(original + unrelated) == modified + unrelated
        for boundary in ("MEM32(esi + 0xE8)", "PUSH32(esp, edi);\n    if (CMP_EQ(_fa, _fb)) goto loc_001EC3AC;"):
            transform(original.replace(boundary, boundary + "BAD"), True)
        transform(original + "\n" + original, True)
        transform(modified, True)
        native = original.replace(OWNER, "original_camera") + "\n" + modified.replace(OWNER, "hidden_camera")
        # Run original minimap direction draws, including native pops/RET20.
        native += "\nvoid original_minimap(void)\n{\n" + minimap[minimap.index("loc_0011DC58: ;"):]
        (folder / "native_arrows.inc").write_text(native)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_hud_cleanup_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)
    print("PASS HUD cleanup seam: only camera Arrow gate changes, native map/route/checkpoint owners unchanged, LF/CRLF and rejected invalid anchors")


if __name__ == "__main__":
    main()
