"""Re-export the repository's checksum-pinned installer; ROOT here is native/gpu/webgpu."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parents[2]
_spec = importlib.util.spec_from_file_location("nonstd_install", REPO / "scripts" / "_install.py")
_shared = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_shared)

CACHE, LOCK = _shared.CACHE, _shared.LOCK
fail, target, digest, download = _shared.fail, _shared.target, _shared.digest, _shared.download
asset_for, extract, install = _shared.asset_for, _shared.extract, _shared.install
