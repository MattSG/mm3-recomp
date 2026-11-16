"""Exercise the production unlock hooks against bounded guest state."""
from pathlib import Path
import subprocess
import os
import shutil
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-unlocks-") as temporary:
        folder = Path(temporary)
        (folder / "recomp_types.h").write_text(
            "#define MEM32(a) (*(uint32_t*)(memory+(a)))\n"
            "#define MEM8(a) memory[(a)]\n"
            "#define XBOX_PTR(a) (memory+(a))\n"
            "#define SET_LO8(r,v) ((r)=((r)&0xffffff00u)|(v))\n"
            "#define PUSH32(sp,v) ((sp)-=4,MEM32(sp)=(v))\n")
        (folder / "xbox_memory_layout.h").write_text("int xbox_GuestMapped(uint32_t,uint32_t);\n")
        (folder / "guest_state.h").write_text(
            "#ifndef TEST_GUEST_STATE_H\n#define TEST_GUEST_STATE_H\n"
            "typedef struct {uint32_t eax,ecx,edx,esp,edi,esi;} xbox_guest_state;\n"
            "void xbox_guest_state_save(xbox_guest_state*);\n"
            "void xbox_guest_state_restore(const xbox_guest_state*,uint32_t);\n#endif\n")
        exe = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command([ROOT / "tests/mm3_unlocks_contract.c"], exe,
                                         includes=[folder], std="c11", warnings="high", werror=True),
                       check=True, cwd=folder)
        for mode in ("6", "8", "invalid"):
            subprocess.run([str(exe)], env=os.environ | {"MM3_EASTER_EGG_EFFECT": mode}, check=True)
        if os.name == "nt":
            car = folder / "car"
            car.mkdir()
            shutil.copyfile(ROOT / "src/mm3_recomp_types.h", car / "recomp_types.h")
            shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", car / "recomp_base.h")
            car_exe = car / "car-contract.exe"
            subprocess.run(cc.compile_command([ROOT / "tests/mm3_car_unlock_contract.c"], car_exe,
                                             includes=[car, ROOT / "tools/xboxrecomp/include"],
                                             defines=["_CRT_SECURE_NO_WARNINGS"],
                                             std="c11", warnings="high", werror=True), check=True, cwd=car)
            subprocess.run([str(car_exe)], check=True)


if __name__ == "__main__":
    main()
