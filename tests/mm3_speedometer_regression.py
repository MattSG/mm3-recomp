"""Verify numeric speed removal and the retained native analog path offline.

Run after scripts/enter-env.ps1. The test extracts the real generated code;
it requires no game process, GPU, XBE data changes or player save.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def generated_function(name):
    for path in (Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))).glob("*.c"):
        text = path.read_text()
        match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", text, re.M | re.S)
        if match:
            return match.group()
    raise ValueError(f"Missing generated {name}")


def numeric_branch(text, name):
    header = text[:text.index("loc_0023F09B:")]
    branch = text[text.index("loc_0023F805:"):text.index("loc_0023F8F8:")]
    # Exercise the readout selector and analog continuation without a GPU.
    return header.replace("sub_0023F09B", name) + branch + """
loc_0023F8F8: ;
    assert(g_esi == renderer);
    ++needle_calls;
loc_0023FB95: ;
    return;
    #undef fp_push
    #undef fp_pop
    #undef fp_top
    #undef fp_st
    #undef fp_st1
}
"""


def needle_branch(text, name):
    header = text[:text.index("loc_0023F09B:")]
    branch = text[text.index("loc_0023F8F8:"):text.index("    g_fp_cmp = RECOMP_FCMP(fp_top(), MEMF(0x384EF4));", text.index("loc_0023F91B:"))]
    return header.replace("sub_0023F09B", name) + branch + """
    ++needle_calls;
    fp_pop();
loc_0023FB95: ;
    return;
    #undef fp_push
    #undef fp_pop
    #undef fp_top
    #undef fp_st
    #undef fp_st1
}
"""


def main():
    if not shutil.which("cl") or not shutil.which("cmake"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe and CMake are available.")
    original = generated_function("sub_0023F09B")
    conversion = generated_function("sub_001F0399")
    with tempfile.TemporaryDirectory(prefix="mm3-speedometer-") as folder:
        folder = Path(folder)
        source, output = folder / "original.c", folder / "modified.c"
        cmake_script = folder / "transform.cmake"
        cmake_script.write_text(
            f'include("{(ROOT / "scripts/mm3_speedometer_seam.cmake").as_posix()}")\n'
            'file(READ "${INPUT}" content)\n'
            'mm3_add_speedometer_readout(content)\n'
            'file(WRITE "${OUTPUT}" "${content}")\n')

        def transform(text, reject=False):
            source.write_bytes(text.encode())
            result = subprocess.run(["cmake", f"-DINPUT={source}", f"-DOUTPUT={output}",
                                     "-P", str(cmake_script)], capture_output=True, text=True)
            if reject:
                assert result.returncode != 0 and "MM3 speedometer readout" in result.stderr
                return None
            if result.returncode:
                raise RuntimeError(result.stderr)
            return output.read_text()

        modified = transform(original)
        assert transform(original.replace("\n", "\r\n")) == modified
        analog = modified[modified.index("loc_0023F8F8:"):]
        analog = analog.replace("    mm3_speedometer_node = ebx;\n", "", 1)
        override = """    if (mm3_speedometer_draw(mm3_speedometer_node, ebp, fp_top())) {
        /* The native getter left one speed value on the x87 stack. */
        fp_pop();
        goto loc_0023FB95;
    }
"""
        assert override in analog
        assert original[original.index("loc_0023F8F8:"):] == analog.replace(override, "", 1)
        # Fail closed on changed/duplicated source, rather than silently
        # applying a HUD branch repair at the wrong location after regeneration.
        transform(original.replace("loc_0023F82D:", "loc_0023F82E:"), reject=True)
        transform(original + "\nloc_0023F82D: ;\n", reject=True)
        transform(original.replace("goto loc_0023F8F8; /* jne: not equal / not zero */",
                                   "goto loc_0023FB95; /* changed */"), reject=True)
        transform(original.replace("loc_0023F91B:", "loc_0023F91C:"), reject=True)
        transform(original.replace("loc_0023F8F8:", "loc_0023F8F9:"), reject=True)
        transform(original + "\nloc_0023F91B: ;\n    g_fp_cmp = RECOMP_FCMP(fp_top(), MEMF(0x384EF4));", reject=True)
        transform(modified, reject=True)
        (folder / "production_speedometer.inc").write_text(
            conversion + "\n" + numeric_branch(original, "contract_original") + "\n" +
            numeric_branch(modified, "contract_modified") + "\n" +
            needle_branch(original, "needle_original") + "\n" +
            needle_branch(modified, "needle_modified"))
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(folder),
                        "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_speedometer_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        subprocess.run([str(executable)], check=True)
    print("PASS: exact CMake HUD seam, hidden numeric speed, analog fallback, override stack balance, LF/CRLF and rejected invalid anchors")


if __name__ == "__main__":
    main()
