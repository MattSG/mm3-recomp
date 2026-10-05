"""mm3view - poke at Midtown Madness 3 game data.

    uv run --with numpy --with pillow tools/mm3view/mm3view.py <command> ...

    ls    [glob]           list assets (loose files, Data_hd.zip, Data_dvd.zip)
    get   <path> [-o dir]  extract one asset as-is
    tex   [glob]           textures/UI (.cdds .dds .tga .raw) -> PNGs + index.html
    model <glob> [n]       car body.cmp + materialSet<n>.omb -> models/<car>_<n>.html
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
    """-> {material: Nx12 float32 rows of pos, normal, uv, specPow, env, amb, specInt},
    unindexed, three rows per triangle.

    A mesh's primitive groups are draw batches; each vertex carries the omb
    material it actually uses, so a triangle takes its first vertex's."""
    w = m["verts"]
    size = m["aabb"][1] - m["aabb"][0]
    pos = np.stack([_bits(w[:, 0], 0, 11) / 1024 * size[0],
                    _bits(w[:, 0], 11, 11) / 1024 * size[1],
                    -_bits(w[:, 0], 22, 10) / 512 * size[2]], 1)
    nrm = np.stack([_bits(w[:, 1], 0, 11) / 1024, _bits(w[:, 1], 11, 11) / 1024,
                    -_bits(w[:, 1], 22, 10) / 512], 1)
    uv = np.stack([_bits(w[:, 2], 0, 11) / 1024, _bits(w[:, 2], 11, 11) / 1024], 1)
    spec = np.stack([(w[:, 5] >> s & 0xFF) / 255 for s in (0, 8, 16, 24)], 1)

    def move(lin, t, sel=slice(None)):
        pos[sel] = pos[sel] @ lin.T + t
        nrm[sel] = nrm[sel] @ lin.T

    if m["skinned"]:
        mid = (w[:, 3] >> 8 & 0xFF) // 37
        for i in np.unique(mid):
            if int(i) in matrices:
                move(*_matrix(matrices[int(i)]), mid == i)
    for lin, t in xforms:
        move(lin, t)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-9

    tris = []
    for (strip, off, cnt), _batch in m["prims"]:
        if strip:
            idx = np.arange(off, off + cnt + 3)
            t = np.stack([idx[:-2], idx[1:-1], idx[2:]], 1)
            t[1::2] = t[1::2, ::-1]
        else:
            t = m["indices"][off:off + (cnt + 1) * 3].reshape(-1, 3).astype(np.int64)
        tris.append(t[(t[:, 0] != t[:, 1]) & (t[:, 1] != t[:, 2]) & (t[:, 0] != t[:, 2])])
    t = np.concatenate(tris)
    rows = np.concatenate([pos, nrm, uv, spec], 1).astype(np.float32)
    mat = (w[t[:, 0], 3] & 0xFF) // 11
    return {int(k): rows[t[mat == k].reshape(-1)] for k in np.unique(mat)}


def texture_key(data, folder, name):
    """omb texture path ('+Cars\\NewBeetle\\x.dds') -> data key, or None."""
    stem = name.lstrip("+").replace("\\", "/").rsplit(".", 1)[0].lower()
    for k in (f"{stem}.cdds", f"{stem}.dds", f"{folder}/{stem.rsplit('/', 1)[-1]}.cdds"):
        if k in data.files:
            return k
    print(f"texture {name} not found", file=sys.stderr)


def material_image(tex, mat):
    """Bake an omb material's colour, texture and mode into one RGBA image,
    as cmpviewer's fragment shader combines them."""
    color = np.array(mat["color"], np.float32) / 255
    if tex is None:
        return Image.new("RGBA", (1, 1), tuple(mat["color"]))
    t = np.asarray(tex, np.float32) / 255
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


def export_model(data, key, mset, out_dir):
    folder = key.rsplit("/", 1)[0]
    omb = f"{folder}/materialset{mset}.omb"
    mats = read_omb(data.read(omb)) if omb in data.files else []
    if not mats:
        print(f"{omb} not found, drawing grey", file=sys.stderr)
    groups = {}
    for path, m, xforms, matrices in read_cmp(data.read(key)):
        for mat, rows in mesh_triangles(m, xforms, matrices).items():
            groups.setdefault(mat, []).append(rows)

    # Every texture in the car's folder, plus shared ones its materials name.
    keys = sorted(k for k in data.glob(f"{folder}/*") if k.endswith("dds"))
    keys += [texture_key(data, folder, m["texture"]) for m in mats if m["texture"].lower() != "no"]
    textures = {k: load_image(k, data.read(k)) for k in dict.fromkeys(keys) if k}

    parts = []
    for mat, rows in sorted(groups.items()):
        rows = np.concatenate(rows)
        om = mats[mat] if mat < len(mats) else dict(name=f"#{mat}", texture="No", color=(128, 128, 128, 255), mode=0)
        tk = texture_key(data, folder, om["texture"]) if om["texture"].lower() != "no" else None
        img = material_image(textures.get(tk), om)
        parts.append(dict(
            name=om["name"], texture=tk, mode=("decal", "transparency", "replace", "modulate")[om["mode"] & 3],
            color="#%02x%02x%02x%02x" % tuple(om["color"]), tris=len(rows) // 3,
            rows=base64.b64encode(rows.tobytes()).decode(), tex=_b64(img),
            alpha=bool(np.asarray(img)[..., 3].min() < 255),
            # ponytail: per-material averages of the per-vertex spec/env bytes;
            # a custom shader could use them per vertex.
            spec=rows[:, 8:12].mean(0).round(3).tolist()))
    out = out_dir / f"{folder.rsplit('/', 1)[-1]}_{mset}.html"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(MODEL_HTML.replace("%TITLE%", key).replace("%PARTS%", json.dumps(parts)).replace(
        "%TEXTURES%", json.dumps({k: _b64(v) for k, v in textures.items()})), encoding="utf-8")
    print(f"{key}: {len(parts)} materials, {len(textures)} textures -> {out}")


def cmd_model(data, args):
    out_dir = args.out / "models"
    for key in sorted(data.glob(args.cmp)):
        export_model(data, key, args.set, out_dir)
    pages = sorted(p.name for p in out_dir.glob("*.html") if p.name != "index.html")
    (out_dir / "pages.js").write_text("pages = " + json.dumps(pages))
    (out_dir / "index.html").write_text(
        "<!doctype html><title>mm3 models</title><style>body{background:#333;color:#ccc;font:13px monospace}"
        "a{color:#8cf}</style>\n" + "<br>\n".join(f'<a href="{p}">{p[:-5]}</a>' for p in pages))


MODEL_HTML = """<!doctype html><meta charset=utf-8><title>%TITLE%</title>
<style>body{margin:0;overflow:hidden;font:12px monospace;color:#ccc;background:#404850}
#ui{position:absolute;top:4px;left:4px;bottom:4px;width:300px;background:#000a;padding:6px;overflow:auto}
#ui h3{margin:8px 0 4px;font-size:12px;color:#fff}
.row{display:flex;align-items:center;gap:4px;padding:1px 0}.row:hover{background:#fff2}
.sw{width:28px;height:28px;flex:none;object-fit:contain;border:1px solid #666;cursor:zoom-in;
background:repeating-conic-gradient(#555 0 25%,#777 0 50%) 0/8px 8px}
.sw:hover{border-color:#fff}small{color:#888}select{width:100%}
#zoom{display:none;position:absolute;inset:0;background:#000c;cursor:zoom-out;align-items:center;
justify-content:center;flex-direction:column}#zoom img{max-width:90vw;max-height:85vh;min-width:256px;
image-rendering:pixelated;background:repeating-conic-gradient(#555 0 25%,#777 0 50%) 0/16px 16px}</style>
<div id=ui><select id=pick></select><b>%TITLE%</b><br><small>drag orbit · wheel zoom · right-drag pan<br>
hover a row to highlight it on the car · click a swatch to enlarge</small>
<label class=row><input type=checkbox id=wire> wireframe</label>
<h3>Materials</h3><div id=mats></div><h3>Textures</h3><div id=texs></div></div>
<div id=zoom><img><div></div></div>
<script src=pages.js></script>
<script type=importmap>{"imports":{"three":"https://unpkg.com/three@0.160.0/build/three.module.js",
"three/addons/":"https://unpkg.com/three@0.160.0/examples/jsm/"}}</script>
<script type=module>
import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {RoomEnvironment} from 'three/addons/environments/RoomEnvironment.js';
const parts = %PARTS%, textures = %TEXTURES%, $ = id => document.getElementById(id);
const f32 = s => new Float32Array(Uint8Array.from(atob(s), c => c.charCodeAt(0)).buffer);
$('zoom').onclick = () => $('zoom').style.display = 'none';
const swatch = (src, title) => {
  const i = Object.assign(new Image(), {src, title, className: 'sw'});
  i.onclick = e => { e.preventDefault(); $('zoom').style.display = 'flex';
    $('zoom').querySelector('img').src = src; $('zoom').querySelector('div').textContent = title; };
  return i;
};
for (const p of window.pages || []) $('pick').add(new Option(p.slice(0, -5), p, false, location.pathname.endsWith(p)));
$('pick').onchange = e => location = e.target.value;

const renderer = new THREE.WebGLRenderer({antialias: true});
document.body.append(renderer.domElement);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x404850);
scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
const sun = new THREE.DirectionalLight(0xffffff, 2); sun.position.set(3, 5, 4); scene.add(sun);
const root = new THREE.Group(); scene.add(root);
const glow = pick => root.children.forEach((o, i) => o.material.emissive.set(pick(parts[i]) ? 0x884400 : 0));

for (const p of parts) {
  const rows = new THREE.InterleavedBuffer(f32(p.rows), 12), g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.InterleavedBufferAttribute(rows, 3, 0));
  g.setAttribute('normal', new THREE.InterleavedBufferAttribute(rows, 3, 3));
  g.setAttribute('uv', new THREE.InterleavedBufferAttribute(rows, 2, 6));
  const map = new THREE.TextureLoader().load(p.tex);
  map.flipY = false;  // D3D UVs: v=0 is the top row
  map.wrapS = map.wrapT = THREE.RepeatWrapping; map.colorSpace = THREE.SRGBColorSpace;
  const [specPow, env, , specInt] = p.spec;
  // env is a reflection amount (car paint has more than chrome), so it drives a clear coat.
  const mat = new THREE.MeshPhysicalMaterial({map, side: THREE.DoubleSide, transparent: p.alpha,
    depthWrite: !p.alpha, roughness: 1 - 0.85 * specPow * Math.sqrt(specInt), metalness: 0,
    clearcoat: env, clearcoatRoughness: 0.1});
  const mesh = new THREE.Mesh(g, mat); root.add(mesh);
  const row = document.createElement('label'); row.className = 'row';
  row.innerHTML = `<input type=checkbox checked><span>${p.name}<br><small>${p.mode} ${p.color} ${p.tris} tris<br>${p.texture || 'no texture'}</small></span>`;
  row.prepend(swatch(p.tex, `${p.name}: ${p.texture || p.color} baked as ${p.mode}`));
  row.querySelector('input').onchange = e => mesh.visible = e.target.checked;
  row.onmouseenter = () => glow(q => q === p); row.onmouseleave = () => glow(() => false);
  $('mats').append(row);
}
for (const [k, src] of Object.entries(textures)) {
  const row = document.createElement('div'); row.className = 'row';
  const sw = swatch(src, k); row.append(sw, k.split('/').pop());
  sw.onload = () => row.append(Object.assign(document.createElement('small'), {textContent: ` ${sw.naturalWidth}x${sw.naturalHeight}`}));
  row.onmouseenter = () => glow(q => q.texture === k); row.onmouseleave = () => glow(() => false);
  $('texs').append(row);
}
$('wire').onchange = e => root.children.forEach(o => o.material.wireframe = e.target.checked);

const box = new THREE.Box3().setFromObject(root), size = box.getSize(new THREE.Vector3()).length();
const camera = new THREE.PerspectiveCamera(45, innerWidth / innerHeight, size / 100, size * 10);
camera.position.copy(box.getCenter(new THREE.Vector3())).add(new THREE.Vector3(-size * 0.9, size * 0.4, size * 0.9));
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
