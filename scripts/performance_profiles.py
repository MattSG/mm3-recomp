"""The native game's profile rows, shared with launch and benchmark tools."""
import ast
from pathlib import Path

PROFILE_FILE = Path(__file__).resolve().parents[1] / "src/mm3_performance_profiles.def"
FIELDS = ("render_scale", "msaa", "draw_distance", "aniso", "fps_limit")


def load_profiles(path=PROFILE_FILE):
    profiles = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        if not line.startswith("MM3_PROFILE(") or not line.endswith(")"):
            raise ValueError(f"{path}:{number}: expected MM3_PROFILE row")
        row = ast.literal_eval(line[len("MM3_PROFILE"):])
        if not isinstance(row, tuple) or len(row) != 6 or not all(isinstance(v, str) for v in row):
            raise ValueError(f"{path}:{number}: expected six strings")
        name, *values = row
        if name in profiles:
            raise ValueError(f"{path}:{number}: duplicate profile {name}")
        profiles[name] = dict(zip(FIELDS, values))
    return profiles
