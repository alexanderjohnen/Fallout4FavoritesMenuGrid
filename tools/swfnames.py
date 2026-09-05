"""List the symbols a Scaleform movie exports, and check a tag configuration
against them.

The icons FIS puts on items live in an icon library movie, and a menu draws
one by attaching the symbol with the right name. So two questions come up
again and again: what does a library export, and does a given tag actually
find its icon in there.

    py -3 tools/swfnames.py names FallUI_IconLib.swf
    py -3 tools/swfnames.py names FallUI_IconLib.swf --filter Med
    py -3 tools/swfnames.py tags "ItemSorter/FIS (FallUI Item Sorter).xml"

Paths are taken as they are, or looked for under Data\\Interface when they
are not found. Reading only.
"""

from __future__ import annotations

import argparse
import io
import re
import struct
import sys
import zlib
from pathlib import Path

INTERFACE = Path(
    r"G:\Program Files (x86)\Steam\steamapps\common\Fallout 4\Data\Interface")

# A symbol exported as "Some.Icon" is attached as "m_Some.Icon": the movie
# clip prefix DEF_UI and FallUI use throughout.
CLIP_PREFIX = "m_"


def resolve(path: str) -> Path:
    candidate = Path(path)
    return candidate if candidate.exists() else INTERFACE / path


def uncompress(raw: bytes) -> bytes:
    """A movie is stored plain (FWS), zlib'd (CWS) or lzma'd (ZWS)."""
    signature, body = raw[:3], raw[8:]
    if signature == b"CWS":
        return zlib.decompress(body)
    if signature == b"ZWS":
        import lzma

        return lzma.decompress(body[4:])
    if signature == b"FWS":
        return body
    raise SystemExit("not a Scaleform movie")


def exported_symbols(path: Path) -> dict[int, str]:
    """Character id -> exported name, from ExportAssets and SymbolClass."""
    body = uncompress(path.read_bytes())
    stream = io.BytesIO(body)

    # The header opens with a rectangle whose size is in its first five bits.
    first = stream.read(1)[0]
    stream.seek(0)
    stream.read((5 + (first >> 3) * 4 + 7) // 8)
    stream.read(4)  # frame rate and frame count

    names: dict[int, str] = {}
    while True:
        head = stream.read(2)
        if len(head) < 2:
            break
        packed = struct.unpack("<H", head)[0]
        code, length = packed >> 6, packed & 0x3F
        if length == 0x3F:
            length = struct.unpack("<I", stream.read(4))[0]
        data = stream.read(length)

        if code in (56, 76):  # ExportAssets, SymbolClass
            entries = io.BytesIO(data)
            for _ in range(struct.unpack("<H", entries.read(2))[0]):
                identifier = struct.unpack("<H", entries.read(2))[0]
                name = b""
                while True:
                    character = entries.read(1)
                    if character in (b"\0", b""):
                        break
                    name += character
                names[identifier] = name.decode("utf-8", "replace")
        if code == 0:
            break
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("what", choices=["names", "tags"])
    parser.add_argument("path")
    parser.add_argument("--filter", default="")
    parser.add_argument(
        "--library",
        default="FallUI_IconLib.swf",
        help="which icon library the tags are checked against")
    arguments = parser.parse_args()

    if arguments.what == "names":
        names = exported_symbols(resolve(arguments.path))
        shown = sorted(
            name for name in names.values() if arguments.filter in name)
        for name in shown:
            print(name)
        print(f"-- {len(shown)} of {len(names)} symbols")
        return 0

    # tags: every <tag keyword="..." icon="..."> of an item sorter, checked
    # against what the library actually has.
    text = resolve(arguments.path).read_text(encoding="utf-8", errors="replace")
    tags = re.findall(r'<tag\s+keyword="([^"]+)"\s+icon="([^"]+)"', text)
    library = set(exported_symbols(resolve(arguments.library)).values())

    missing = []
    for keyword, icon in tags:
        if CLIP_PREFIX + icon in library:
            if arguments.filter in keyword:
                print(f"{keyword:<24} {CLIP_PREFIX}{icon}")
        else:
            missing.append((keyword, icon))

    print(f"-- {len(tags) - len(missing)} of {len(tags)} tags find their icon")
    for keyword, icon in missing:
        print(f"   missing: {keyword} -> {CLIP_PREFIX}{icon}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
