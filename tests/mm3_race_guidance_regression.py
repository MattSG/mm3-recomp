"""Exercise shared race guidance against the native marker projection/ABI."""
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile
import math

from mm3_map_zoom_regression import constants

ROOT = Path(__file__).resolve().parents[1]
OWNERS = ("sub_00201BCA", "sub_0011CAE8", "sub_00120091", "sub_00116535",
          "sub_0014BAE9", "sub_0014C121")
WORLD = "sub_0020C9F9"
MATH = ("sub_0002DC95", "sub_0006ACC3", "sub_000C6ADC", "sub_0006AC61",
        "sub_0006AE0C", "sub_0006AEE2", "sub_0001259E", "sub_0006AD9F", "sub_0006AE60",
        "sub_0020BF9C", "sub_0001BA6A", "sub_001C1585", "sub_0011964B",
        "sub_00069807", "sub_001195F7", "sub_0006B623", "sub_000C6CD0",
        "sub_00349470", "sub_0034C0E0", "sub_000C191C", "sub_0004DBC7")
HOOKS = {
    OWNERS[0]: ("loc_00201BCA", "loc_00201CC0", "loc_00201D55"),
    OWNERS[1]: ("loc_0011CB45",),
    OWNERS[2]: ("loc_00120091",),
    OWNERS[3]: ("loc_00116535", "loc_001171D4", "loc_0011726E", "loc_0011731C"),
    OWNERS[4]: ("loc_0014C10F",),
    OWNERS[5]: ("loc_0014C121",),
}


def retail_city_fixture(city, destination):
    """Decode the retail road/junction sections using the native load formats.

    144C9D loads templates, 1554D8/138CFC load road splines, 138C4C loads
    junctions and 14E660 resolves the paired object IDs into native pointers.
    Build only fields read by the production snapshot; preserve retail curves
    and connectivity rather than committing copies of the game's assets.
    """
    data = (ROOT / "game_files/Data/Shared" / city / "city.ai").read_bytes()
    cursor = data.index(b"\0") + 1
    base = data.index(b"\0", cursor) + 1 + 80
    counts = struct.unpack_from("<8I", data, base)
    offsets = struct.unpack_from("<8I", data, base + 32)
    _, ordinary, special, junctions, areas, _, _, link_count = counts
    cursor = base + offsets[1]

    def u32():
        nonlocal cursor
        result = struct.unpack_from("<I", data, cursor)[0]
        cursor += 4
        return result

    def floats(count):
        nonlocal cursor
        result = struct.unpack_from("<" + "f" * count, data, cursor)
        cursor += count * 4
        return result

    def skip_array(stride):
        nonlocal cursor
        count = u32()
        cursor += count * stride

    def common():
        nonlocal cursor
        matrix = floats(15)
        for _ in range(2):
            count = u32()
            cursor += count * 4
        return matrix[9:12]

    roads = []
    for index in range(ordinary + special):
        common()
        length, = floats(1)
        parameters = floats(3)
        controls = floats(u32() * 3)
        skip_array(4)
        cursor += 4
        skip_array(4)
        skip_array(2)
        skip_array(2)
        cursor += 12
        roads.append((length, parameters, controls))
        if index + 1 == ordinary:
            assert cursor == base + offsets[2]
    assert cursor == base + offsets[3]
    centers = []
    for _ in range(junctions):
        centers.append(common())
        cursor += 48
        u32()
        if u32() == 1:
            cursor += 16
            signals = u32()
            for _ in range(signals):
                skip_array(4)
                skip_array(6)
    assert cursor == base + offsets[4]
    link_ids = struct.unpack_from("<" + "i" * (2 * link_count), data, base + offsets[7])
    assert all(-1 <= index < ordinary + special + junctions + areas for index in link_ids)

    address_base, memory = 0x100000, bytearray()

    def allocate(size):
        padding = (-len(memory)) % 16
        memory.extend(bytes(padding + size))
        return address_base + len(memory) - size

    def write(address, fmt, *values):
        struct.pack_into("<" + fmt, memory, address - address_base, *values)

    manager = allocate(0xD0)
    arrays = [allocate(count * stride) for count, stride in
              ((ordinary, 0xDC), (special, 0xDC), (junctions, 0xAC), (areas, 0xE8))]
    for group, count in enumerate((ordinary, special, junctions, areas)):
        write(manager + 0x40 + 4 * group, "I", arrays[group])
        write(manager + 0xA0 + 4 * group, "I", count)
    objects = [arrays[group] + index * stride
               for group, count, stride in ((0, ordinary, 0xDC), (1, special, 0xDC),
                                             (2, junctions, 0xAC), (3, areas, 0xE8))
               for index in range(count)]
    for index, (length, parameters, controls) in enumerate(roads):
        spline = allocate(0x1C)
        points = allocate(len(controls) * 4)
        write(points, "f" * len(controls), *controls)
        write(spline + 4, "II", points, points + len(controls) * 4)
        write(spline + 0x10, "3f", *parameters)
        write(objects[index] + 0x74, "fI", length, spline)
    for index, center in enumerate(centers):
        write(arrays[2] + index * 0xAC + 0x34, "3f", *center)
    links = allocate(link_count * 8)
    write(links, "I" * len(link_ids), *(objects[index] if index >= 0 else 0 for index in link_ids))
    write(manager + 0x58, "I", links)
    write(manager + 0xB0, "I", link_count)
    expected_nodes = junctions + sum(math.ceil(length / 6) + 1 for length, _, _ in roads[:ordinary])
    street_ids = set(range(ordinary)) | set(range(ordinary + special, ordinary + special + junctions))
    adjacency = {index: set() for index in street_ids}
    for a, b in zip(link_ids[::2], link_ids[1::2]):
        if a in street_ids and b in street_ids:
            adjacency[a].add(b)
            adjacency[b].add(a)
    visited, pending = set(), [0]
    while pending:
        current = pending.pop()
        if current not in visited:
            visited.add(current)
            pending.extend(adjacency[current] - visited)
    expected_reachable = sum(math.ceil(roads[index][0] / 6) + 1 if index < ordinary else 1
                             for index in visited)
    destination.write_bytes(struct.pack("<7I", address_base, manager, len(memory), ordinary,
                                        junctions, expected_nodes, expected_reachable) + memory)
    return destination


def ui_declaration_data():
    """Use retail shader tokens and XDK tables, not assumed renderer formats."""
    with zipfile.ZipFile(ROOT / "game_files/Data/Data_hd.zip") as archive:
        declarations = []
        for name in ("Meme/Vertex.cvsx", "Meme/ScaledVertex.cvsx"):
            shader = archive.read(name)
            assert shader[:4] == b"CVSX"
            count = struct.unpack_from("<I", shader, 12)[0]
            declarations.append(struct.unpack_from("<" + "I" * count, shader, 16))
        assert declarations[0] == declarations[1]
        material = archive.read("Material/Meme.cdma")
        # Native EDA6B hashes "polygon" to E8EAB3AA with this file's seed 7.
        # That compiled prototype uses external shader resource 2:
        # the second name in its table is +Meme/Vertex.vsh.
        count = struct.unpack_from("<H", material, 10)[0]
        entries = [struct.unpack_from("<II", material, 16 + i * 8) for i in range(count)]
        polygon = next(offset for name_hash, offset in entries if name_hash == 0xE8EAB3AA)
        assert struct.unpack_from("<I", material, polygon + 0x28 + 0x44 + 0x64)[0] == 2
        strings = material[struct.unpack_from("<H", material, 14)[0]:].split(b"\0")
        assert strings[1] == b"+Meme/Vertex.vsh"
    xbe = (ROOT / "game_files/default.xbe").read_bytes()
    u32 = lambda offset: struct.unpack_from("<I", xbe, offset)[0]
    sections = [struct.unpack_from("<4I", xbe, u32(0x120) - u32(0x104) + i * 56 + 4)
                for i in range(u32(0x11C))]
    lines = [f"    MEM32(0x{0x6D000 + i * 4:X}) = 0x{token:08X}u;"
             for i, token in enumerate(declarations[0])]
    for address, count in ((0x3AEB68, 32), (0x34FBE0, 16)):
        section = next(s for s in sections if s[0] <= address <= s[0] + s[3] - count)
        data = xbe[section[2] + address - section[0]:section[2] + address - section[0] + count]
        lines += [f"    MEM8(0x{address + i:X}) = {byte};" for i, byte in enumerate(data)]
    return "\nstatic void native_ui_declaration_data(void) {\n" + "\n".join(lines) + "\n}\n"


def main():
    if not shutil.which("cl") or not shutil.which("cmake"):
        raise SystemExit("Run scripts/enter-env.ps1 first.")
    native = {}
    for path in sorted((ROOT / "src/recomp/gen").glob("recomp_*.c")):
        source = path.read_text()
        for name in OWNERS + MATH + (WORLD,):
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in native, name
                native[name] = match.group()
    assert set(native) == set(OWNERS + MATH + (WORLD,))
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert "list(APPEND MM3_SEAM_FUNCTIONS " + " ".join(OWNERS) + ")" in cmake
    assert "mm3_add_race_guidance(distance_content ${race_owner})" in cmake
    assert "src/mm3_race_guidance.c" in cmake
    assert WORLD not in re.search(r"foreach\(race_owner ([^)]+)\)", cmake).group(1)
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
        declarations = "\n".join("void " + name + "(void);" for name in MATH + OWNERS)
        body = "\n".join(native[name].replace("void sub_001C1585", "void native_submit", 1)
                         if name == "sub_001C1585" else native[name] for name in MATH)
        marker = OWNERS[1]
        body += "\n" + native[marker].replace("void " + marker, "void retail_marker", 1)
        body += "\n" + modified[marker]
        body += "\n" + native[WORLD]
        body += "\n" + modified[OWNERS[2]]
        body += "\n" + modified[OWNERS[0]]
        body += "\n" + modified[OWNERS[3]]
        # Native vertex quantization biases live in the XBE's zero-filled
        # writable tail, not its raw section bytes. This fixture explicitly
        # initializes both in native_submission_begin instead of inventing
        # retail file constants for them.
        file_constants = re.sub(r"MEMF\(0x4085F[8C]\)", "runtime_quantization_bias", body)
        (folder / "native_race_guidance.inc").write_text(
            declarations + "\n" + body + constants(file_constants) + ui_declaration_data())
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "src/mm3_race_guidance.c", folder / "production_race_guidance.c")
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_race_guidance_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        fixtures = [retail_city_fixture(city, folder / (city + ".bin")) for city in ("Paris", "Washington")]
        subprocess.run([str(executable), *(str(path) for path in fixtures)], check=True, cwd=folder)
    print("PASS race guidance seams: source owners, LF/CRLF, unchanged native instructions and invalid anchors")


if __name__ == "__main__":
    main()
