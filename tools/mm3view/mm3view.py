"""mm3view - poke at Midtown Madness 3 game data.

    uv run --with numpy --with pillow tools/mm3view/mm3view.py <command> ...

    ls    [glob]           list assets (loose files, Data_hd.zip, Data_dvd.zip)
    get   <path> [-o dir]  extract one asset as-is
    tex   [glob]           textures/UI (.cdds .dds .tga .raw) -> PNGs + index.html
    model <body.cmp> [n]   car model + materialSet<n>.omb -> self-contained .html
    map   <City>           city heightmap cells -> one labelled PNG

Paths are case-insensitive and relative to Data/ (zip members) or
Data/Shared/ (loose files), e.g. cars/a_pmetro/body.cmp. Output goes to
out/mm3view/ unless -o is given.

Formats follow Daniel Stien's reverse engineering in
https://github.com/dstien/gameformats/tree/master/mm3 (cmpviewer,
cityviewer, decdds). CDDS decompression uses his decdds.c as-is: it is
downloaded and compiled into tools/cache/decdds on first use.
"""
import argparse
import base64
import ctypes
import fnmatch
import io
import json
import os
import struct
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
CACHE = REPO / "tools" / "cache" / "decdds"
GF_URL = ("https://raw.githubusercontent.com/dstien/gameformats/"
          "48b78d1676e4c39eaa2584544533efdf3a4a18ba/mm3/decdds/")


# --- asset lookup -----------------------------------------------------------

class Data:
    """One case-insensitive namespace over the loose files and both zips."""

    def __init__(self, root):
        self.files = {}  # lowercase key -> (zipfile or None, real name/path)
        for p in sorted((root / "Shared").rglob("*")):
            if p.is_file():
                self.files.setdefault(p.relative_to(root / "Shared").as_posix().lower(), (None, p))
        for name in ("Data_hd.zip", "Data_dvd.zip"):
            z = self._zip(root / name)
            for i in z.infolist() if z else ():
                self.files.setdefault(i.filename.lower(), (z, i.filename))

    @staticmethod
    def _zip(path):
        try:
            return zipfile.ZipFile(path)
        except PermissionError:
            # A running mm3_recomp holds Data_dvd.zip open; read the disc copy.
            iso = next((REPO / "games" / "xiso").glob("*.iso"), None)
            if not iso:
                print(f"warning: {path.name} is locked and no XISO found, skipped", file=sys.stderr)
                return None
            sys.path.insert(0, str(REPO / "tools" / "xboxrecomp" / "tools" / "xiso"))
            from xdvdfs import Xiso
            with Xiso(str(iso)) as x:
                return zipfile.ZipFile(io.BytesIO(x.read(x.find(path.name, "Data"))))
        except FileNotFoundError:
            return None

    def glob(self, pattern):
        return [k for k in self.files if fnmatch.fnmatch(k, pattern.lower())]

    def read(self, key):
        z, name = self.files[key.lower()]
        return z.read(name) if z else name.read_bytes()

    def size(self, key):
        z, name = self.files[key.lower()]
        return z.getinfo(name).file_size if z else name.stat().st_size

    def source(self, key):
        z, _ = self.files[key.lower()]
        return Path(z.filename).name if z and isinstance(z.filename, str) else "Data_dvd.zip (xiso)" if z else "loose"


# --- textures ---------------------------------------------------------------

def _decdds():
    dll = CACHE / ("decdds.dll" if os.name == "nt" else "decdds.so")
    if not dll.exists():
        CACHE.mkdir(parents=True, exist_ok=True)
        for f in ("decdds.c", "decdds.h"):
            urllib.request.urlretrieve(GF_URL + f, CACHE / f)
        if os.name == "nt":
            vs = subprocess.check_output([
                os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"),
                "-latest", "-property", "installationPath"], text=True).strip()
            bat = CACHE / "build.bat"
            bat.write_text(f'@call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\r\n'
                           "cl /nologo /LD /O2 decdds.c /Fe:decdds.dll /link /EXPORT:decdds_extract\r\n")
            subprocess.check_call(["cmd", "/c", str(bat)], cwd=CACHE, stdout=subprocess.DEVNULL)
        else:
            subprocess.check_call(["cc", "-O2", "-shared", "-fPIC", "-o", dll.name, "decdds.c"], cwd=CACHE)
    lib = ctypes.CDLL(str(dll))
    lib.decdds_extract.argtypes = [ctypes.c_char_p, ctypes.c_uint32,
                                   ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
                                   ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
    return lib


def cdds_to_dds(data, _lib=[]):
    if not _lib:
        _lib.append(_decdds())
    out, n = ctypes.POINTER(ctypes.c_uint8)(), ctypes.c_uint32()
    err = _lib[0].decdds_extract(data, len(data), ctypes.byref(out), ctypes.byref(n), 0)
    if err:
        raise ValueError(f"decdds error {err}")
    return ctypes.string_at(out, n.value)  # ponytail: leaks the malloc'd copy; fine for a CLI run


def load_image(key, data):
    """Any game texture -> RGBA PIL image. Cube/volume maps show their first face."""
    ext = key.rsplit(".", 1)[-1]
    if ext == "cdds":
        data = cdds_to_dds(data)
    if ext == "raw":
        # LoadmeterTexture/PressStart: headerless linear A8R8G8B8, 256 wide.
        return Image.frombytes("RGBA", (256, len(data) // 1024), data, "raw", "BGRA")
    return Image.open(io.BytesIO(data)).convert("RGBA")


def cmd_tex(data, args):
    out = args.out / "tex"
    rows = []
    for key in sorted(data.glob(args.glob)):
        if key.rsplit(".", 1)[-1] not in ("cdds", "dds", "tga", "raw"):
            continue
        try:
            img = load_image(key, data.read(key))
        except Exception as e:
            print(f"{key}: {e}", file=sys.stderr)
            continue
        png = out / (key + ".png")
        png.parent.mkdir(parents=True, exist_ok=True)
        img.save(png)
        rows.append(f'<figure><a href="{key}.png"><img src="{key}.png" loading="lazy"></a>'
                    f"<figcaption>{key}<br>{img.width}x{img.height}</figcaption></figure>")
    (out / "index.html").write_text(
        "<!doctype html><title>mm3 textures</title><style>"
        "body{background:#333;color:#ccc;font:11px monospace;display:flex;flex-wrap:wrap}"
        "figure{margin:4px;width:160px}img{max-width:160px;max-height:160px;"
        "background:repeating-conic-gradient(#555 0 25%,#777 0 50%) 0/16px 16px}"
        "figcaption{word-break:break-all}</style>\n" + "\n".join(rows))
    print(f"{len(rows)} textures -> {out / 'index.html'}")


# --- car models (.cmp + .omb) -----------------------------------------------

class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def take(self, n):
        self.p += n
        return self.d[self.p - n:self.p]

    def u(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.p)
        self.p += struct.calcsize("<" + fmt)
        return v if len(v) > 1 else v[0]

    def str(self):
        end = self.d.index(b"\0", self.p)
        s, self.p = self.d[self.p:end].decode("latin-1"), end + 1
        return s


def read_omb(data):
    r = Reader(data)
    assert r.u("B") == 0, "unknown material set header"
    mats = []
    for _ in range(r.u("I")):
        name, tex, _unk = r.str(), r.str(), r.u("B")
        b, g, red, a = r.u("4B")
        mats.append(dict(name=name, texture=tex, color=(red, g, b, a), mode=r.u("I")))
    return mats


def _bits(w, off, n):
    """Signed bitfield n bits wide at bit off (MSVC/GCC little-endian layout)."""
    return ((w << (32 - off - n)).astype(np.uint32).view(np.int32) >> (32 - n)).astype(np.float32)


def _matrix(m):
    """cmp 4x3 -> (3x3 linear, translation), as cmpviewer's cmpMatrix2osgMatrix."""
    a = np.array(m, np.float32).reshape(4, 3)
    return a[:3], a[3] * (1, 1, -1)


def read_cmp(data):
    """Returns [(node path, MeshData dict, [transforms innermost first])] for LOD 0."""
    r = Reader(data)
    assert r.u("I") == 0, "not a cmp root"
    ver = r.u("I")
    assert ver in (109, 114, 115), f"unknown cmp version {ver}"
    meshes, matrices = [], {}

    def transformation():
        _flags, world, rel = r.u("I"), r.u("12f"), r.u("12f")
        r.take(16)
        return world, rel

    def mesh_data():
        m = dict(name=r.str(), length=r.u("I"))
        if not m["length"]:
            return m
        start = r.p
        r.take(4)
        m["aabb"] = np.array(r.u("6f"), np.float32).reshape(2, 3)
        vcount, icount = r.u("2I")
        r.take(12 + (16 if ver >= 115 else 4))
        r.str()
        if r.u("B"):
            _unk, ilen = r.u("2I")
            m["indices"] = np.frombuffer(r.take(ilen), np.uint16)
        r.take(5)
        assert r.u("I") == vcount and r.u("I") == 24
        vlen, _unk = r.u("2I")
        m["verts"] = np.frombuffer(r.take(vlen), np.uint32).reshape(-1, 6)
        prims = []
        for _ in range(r.u("I") // 2):
            kind = r.u("H")
            if kind == 0x6001:
                r.take(4)
            elif kind != 0x8801:
                raise ValueError(f"unknown primitive {kind:#x} in {m['name']}")
            off, cnt = r.u("2H")
            r.take(10)
            prims.append((kind == 0x8801, off, cnt))
        mats = [r.u("4IBI")[5] for _ in range(r.u("I"))]
        m["prims"] = list(zip(prims, mats))
        if ver >= 115 and r.u("I"):
            r.take(28 * r.u("I"))
        if r.p - start != m["length"]:  # Ambient_Bus has 8 unknown trailing bytes
            print(f"mesh {m['name']}: parsed {r.p - start} of {m['length']} bytes", file=sys.stderr)
            r.p = start + m["length"]
        return m

    def node(path, xforms):
        kind = r.u("I")
        name = r.str()
        path = f"{path}/{name}"
        if kind == 1:  # transform
            world, rel = transformation()
            mid = r.u("i")
            if mid >= 0:
                matrices[mid] = world
            r.take(24)
            children(path, [_matrix(rel)] + xforms)
        elif kind in (2, 6):  # mesh, multimesh
            r.take(16)
            if kind == 6:
                r.take(24)
            for lod in range(3 if kind == 6 else 2):
                if not r.u("B"):
                    break
                m = mesh_data()
                if lod == 0:
                    m["skinned"] = kind == 6
                    meshes.append((path, m, xforms))
        elif kind == 4:  # light
            r.take(32 + (8 if ver >= 114 else 0))
        elif kind == 5:  # smoke
            r.take(4)
        elif kind != 3:  # axis has nothing
            raise ValueError(f"unknown node type {kind}")

    def children(path, xforms):
        for _ in range(r.u("I")):
            node(path, xforms)

    r.str()
    r.take(4 + 24 + 4 + 2 + 1)
    if ver >= 114:
        r.str()
    transformation()
    r.take(4 + 1 + 24)
    entries = r.u("I")
    r.take(12 + 12 + 16 * entries + 4)
    children("", [])

    # Meshes with no data reuse an earlier one of the same name.
    by_name = {m["name"]: m for _, m, _ in meshes if m["length"]}
    out = []
    for path, m, xforms in meshes:
        src = m if m["length"] else by_name.get(m["name"])
        if src:
            out.append((path, dict(src, skinned=m["skinned"]), xforms, matrices))
    return out


def mesh_triangles(m, xforms, matrices):
    """-> {material: (positions Nx3, uvs Nx2)} as unindexed triangle soup."""
    w = m["verts"]
    size = m["aabb"][1] - m["aabb"][0]
    pos = np.stack([_bits(w[:, 0], 0, 11) / 1024 * size[0],
                    _bits(w[:, 0], 11, 11) / 1024 * size[1],
                    -_bits(w[:, 0], 22, 10) / 512 * size[2]], 1)
    uv = np.stack([_bits(w[:, 2], 0, 11) / 1024, _bits(w[:, 2], 11, 11) / 1024], 1)
    if m["skinned"]:
        mid = (w[:, 3] >> 8 & 0xFF) // 37
        for i in np.unique(mid):
            if int(i) in matrices:
                lin, t = _matrix(matrices[int(i)])
                pos[mid == i] = pos[mid == i] @ lin.T + t
    for lin, t in xforms:
        pos = pos @ lin.T + t

    tris = {}
    for (strip, off, cnt), mat in m["prims"]:
        if strip:
            idx = np.arange(off, off + cnt + 3)
            t = np.stack([idx[:-2], idx[1:-1], idx[2:]], 1)
            t[1::2] = t[1::2, ::-1]
        else:
            t = m["indices"][off:off + (cnt + 1) * 3].reshape(-1, 3).astype(np.int64)
        t = t[(t[:, 0] != t[:, 1]) & (t[:, 1] != t[:, 2]) & (t[:, 0] != t[:, 2])]
        tris.setdefault(mat, []).append(t.reshape(-1))
    return {k: (pos[np.concatenate(v)], uv[np.concatenate(v)]) for k, v in tris.items()}


def material_image(data, folder, mat):
    """Bake an omb material's colour, texture and mode into one RGBA image,
    as cmpviewer's fragment shader combines them."""
    color = np.array(mat["color"], np.float32) / 255
    solid = Image.new("RGBA", (1, 1), tuple(mat["color"]))
    if mat["texture"].lower() == "no":
        return solid
    stem = mat["texture"].replace("\\", "/").rsplit("/", 1)[-1].rsplit(".", 1)[0].lower()
    keys = data.glob(f"{folder}/{stem}.*dds") or data.glob(f"*/{stem}.*dds")
    if not keys:
        print(f"texture {mat['texture']} not found", file=sys.stderr)
        return solid
    t = np.asarray(load_image(keys[0], data.read(keys[0])), np.float32) / 255
    rgb, a = t[..., :3], t[..., 3:]
    if mat["mode"] == 0:  # decal
        t = np.concatenate([color[:3] * (1 - a) + rgb * a, np.full_like(a, color[3])], -1)
    elif mat["mode"] == 3:  # modulate
        t = np.concatenate([np.where(a == 1, rgb, rgb * color[:3]), np.full_like(a, color[3])], -1)
    # 1 transparency, 2 replace: the texture as-is.
    return Image.fromarray((t * 255).round().astype(np.uint8), "RGBA")


def _b64(img):
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()


def cmd_model(data, args):
    key = args.cmp.lower()
    folder = key.rsplit("/", 1)[0]
    omb = f"{folder}/materialset{args.set}.omb"
    mats = read_omb(data.read(omb)) if omb in data.files else []
    if not mats:
        print(f"{omb} not found, drawing grey", file=sys.stderr)
    groups = {}
    for path, m, xforms, matrices in read_cmp(data.read(key)):
        for mat, (p, uv) in mesh_triangles(m, xforms, matrices).items():
            g = groups.setdefault(mat, ([], []))
            g[0].append(p)
            g[1].append(uv)
        print(f"{path}: {len(m['verts'])} verts")
    parts = []
    for mat, (p, uv) in sorted(groups.items()):
        img = material_image(data, folder, mats[mat]) if mat < len(mats) else Image.new("RGBA", (1, 1), "grey")
        parts.append(dict(
            name=mats[mat]["name"] if mat < len(mats) else str(mat),
            pos=base64.b64encode(np.concatenate(p).astype("<f4").tobytes()).decode(),
            uv=base64.b64encode(np.concatenate(uv).astype("<f4").tobytes()).decode(),
            tex=_b64(img), alpha=bool(np.asarray(img)[..., 3].min() < 255)))
    out = args.out / (key.replace("/", "_") + f"{args.set}.html")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(MODEL_HTML.replace("%TITLE%", key).replace("%PARTS%", json.dumps(parts)))
    print(f"{len(parts)} materials -> {out}")


MODEL_HTML = """<!doctype html><title>%TITLE%</title>
<style>body{margin:0;overflow:hidden;font:12px monospace;color:#ccc}
#ui{position:absolute;top:4px;left:4px;background:#0008;padding:4px;max-height:95vh;overflow:auto}</style>
<div id=ui><b>%TITLE%</b><br>drag: orbit, wheel: zoom, right-drag: pan<br></div>
<script type=importmap>{"imports":{"three":"https://unpkg.com/three@0.160.0/build/three.module.js",
"three/addons/":"https://unpkg.com/three@0.160.0/examples/jsm/"}}</script>
<script type=module>
import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
const parts = %PARTS%;
const f32 = s => new Float32Array(Uint8Array.from(atob(s), c => c.charCodeAt(0)).buffer);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x404850);
scene.add(new THREE.HemisphereLight(0xffffff, 0x444444, 2.5));
const sun = new THREE.DirectionalLight(0xffffff, 1.5); sun.position.set(3, 5, 4); scene.add(sun);
const root = new THREE.Group(); scene.add(root);
for (const p of parts) {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(f32(p.pos), 3));
  g.setAttribute('uv', new THREE.BufferAttribute(f32(p.uv), 2));
  g.computeVertexNormals();
  const map = new THREE.TextureLoader().load(p.tex);
  map.wrapS = map.wrapT = THREE.RepeatWrapping; map.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({map, side: THREE.DoubleSide,
    transparent: p.alpha, depthWrite: !p.alpha}));
  root.add(mesh);
  const l = document.createElement('label');
  l.innerHTML = `<input type=checkbox checked> ${p.name}<br>`;
  l.firstChild.onchange = e => mesh.visible = e.target.checked;
  document.getElementById('ui').append(l);
}
const box = new THREE.Box3().setFromObject(root), size = box.getSize(new THREE.Vector3()).length();
const camera = new THREE.PerspectiveCamera(45, innerWidth / innerHeight, size / 100, size * 10);
camera.position.copy(box.getCenter(new THREE.Vector3())).add(new THREE.Vector3(size, size / 2, size));
const renderer = new THREE.WebGLRenderer({antialias: true});
document.body.append(renderer.domElement);
const controls = new OrbitControls(camera, renderer.domElement);
controls.target.copy(box.getCenter(new THREE.Vector3()));
onresize = () => { renderer.setSize(innerWidth, innerHeight); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); };
onresize();
renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
</script>
"""


# --- city maps (.xbc + .toc + .pak) ------------------------------------------

def cmd_map(data, args):
    city = args.city.lower()
    r = Reader(data.read(f"{city}/{city}.xbc")[:1 << 20])
    assert r.str() == "1.53", "unknown xbc version"
    cols, rows = r.u("2I")
    name = r.str()
    r.take(24 + 16 + 4 + 44 + 28)
    cells = r.u("I")
    r.take(4 * cells)

    toc = Reader(data.read(f"{city}/{city}.toc"))
    toc.take(8)
    entries = sorted(np.frombuffer(toc.take(12 * toc.u("I")), "<u4").reshape(-1, 3)[:, 2].tolist())
    _, pak = data.files[f"{city}/{city}.pak"]
    tiles = {}
    with open(pak, "rb") as f:
        # cityviewer skips the pak's texture entries by parsing the whole xbc
        # for their count; a cell header starting with its own id is enough.
        # ponytail: header sniffing, parse the xbc texture list if it misfires.
        for off in entries:
            f.seek(off)
            head = f.read(140)
            cid, = struct.unpack_from("<I", head)
            hm_off, width = struct.unpack_from("<2I", head, 132)
            if cid != len(tiles) or not 0 < width <= 512:
                continue
            f.seek(off + hm_off)
            tiles[cid] = Image.frombytes("L", (width, width), f.read(width * width))
            if len(tiles) == cells:
                break
    w = tiles[0].width
    img = Image.new("L", (cols * w, rows * w))
    draw = ImageDraw.Draw(img)
    for cid, tile in tiles.items():
        x, y = cid % cols * w, (rows - 1 - cid // cols) * w  # row 0 at the bottom
        img.paste(tile.transpose(Image.FLIP_TOP_BOTTOM), (x, y))  # rows run south to north
        draw.rectangle([x, y, x + w - 1, y + w - 1], outline=128)
        draw.text((x + 2, y + 2), str(cid), fill=255)
    out = args.out / f"{name}_heightmap.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"{len(tiles)}/{cells} cells ({cols}x{rows}) -> {out}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", type=Path, default=REPO / "game_files" / "Data")
    ap.add_argument("-o", "--out", type=Path, default=REPO / "out" / "mm3view")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ls").add_argument("glob", nargs="?", default="*")
    sub.add_parser("get").add_argument("path")
    sub.add_parser("tex").add_argument("glob", nargs="?", default="*")
    p = sub.add_parser("model")
    p.add_argument("cmp")
    p.add_argument("set", nargs="?", default="00")
    sub.add_parser("map").add_argument("city")
    args = ap.parse_args()
    data = Data(args.data)
    if args.cmd == "ls":
        for k in sorted(data.glob(args.glob)):
            print(f"{data.size(k):>10}  {data.source(k):<14} {k}")
    elif args.cmd == "get":
        dest = args.out / args.path.lower()
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data.read(args.path))
        print(dest)
    else:
        globals()["cmd_" + args.cmd](data, args)


if __name__ == "__main__":
    main()
