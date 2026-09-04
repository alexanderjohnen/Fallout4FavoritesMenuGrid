"""Copy a built Favorites Menu Grid straight into Fallout 4.

Skips the archive/mod-manager round trip while the mod is being worked on.
Refuses to copy while the game is running, because Windows keeps a loaded
DLL locked and a half-written plugin is worse than an old one.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILT_DLL = ROOT / "build" / "Release" / "FavoritesMenuGrid.dll"
BUILT_PDB = ROOT / "build" / "Release" / "FavoritesMenuGrid.pdb"

DEFAULT_DATA = Path(
    os.environ.get(
        "FALLOUT4_DATA",
        r"G:\Program Files (x86)\Steam\steamapps\common\Fallout 4\Data",
    )
)


def game_is_running() -> bool:
    """True when Fallout4.exe holds a handle we would fight over."""
    try:
        output = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq Fallout4.exe"],
            capture_output=True,
            text=True,
            check=False,
        ).stdout
    except OSError:
        return False
    return "Fallout4.exe" in output


def main() -> int:
    data = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DATA
    plugins = data / "F4SE" / "Plugins"

    if not BUILT_DLL.is_file():
        print(f"no build found at {BUILT_DLL}", file=sys.stderr)
        return 1
    if not data.is_dir():
        print(f"no Fallout 4 Data folder at {data}", file=sys.stderr)
        return 1
    if game_is_running():
        print("Fallout 4 is running -- close it first", file=sys.stderr)
        return 1

    plugins.mkdir(parents=True, exist_ok=True)
    shutil.copy2(BUILT_DLL, plugins / BUILT_DLL.name)
    print(f"copied {BUILT_DLL.name} -> {plugins}")

    # The PDB is what turns a Buffout crash log into named frames. It costs
    # nothing next to the DLL and is the difference between a usable report
    # and a list of addresses.
    if BUILT_PDB.is_file():
        shutil.copy2(BUILT_PDB, plugins / BUILT_PDB.name)
        print(f"copied {BUILT_PDB.name} -> {plugins}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
