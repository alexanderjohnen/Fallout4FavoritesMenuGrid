"""Write the empty movie the grid is drawn on.

A menu of our own needs a movie of its own -- the game loads one when it
opens a menu, and everything the plugin draws is then a child of its stage.
Nothing of the interface lives in this file: no timeline, no ActionScript, no
graphics. It is a blank sheet of the right size, and the plugin paints on it.

That is deliberate. The alternative is shipping a real interface file, and
then every other mod that ships one for the same menu collides with us. This
one belongs to no vanilla menu at all, so there is nothing to collide with.

Written by hand rather than exported from an authoring tool, because the
whole file is a header and four tags, and a build step nobody can rerun is
worse than forty lines of Python.

    py -3 tools/build_swf.py            writes Interface/FavoritesMenuGrid.swf
    py -3 tools/build_swf.py --check    prints what came out
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "Interface" / "FavoritesMenuGrid.swf"

# The stage both Fallout 4 and Starfield lay their menus out on. Scaleform
# scales it to whatever the screen is, so these are the units every position
# in the plugin is written in.
STAGE_WIDTH = 1280
STAGE_HEIGHT = 720

TWIPS = 20
FRAME_RATE = 60

# Tag codes, from the SWF file format specification.
TAG_END = 0
TAG_SHOW_FRAME = 1
TAG_SET_BACKGROUND_COLOR = 9
TAG_FILE_ATTRIBUTES = 69


def rect(width: int, height: int) -> bytes:
    """The stage size, as the packed bit rectangle a SWF header opens with."""
    xmax = width * TWIPS
    ymax = height * TWIPS
    bits = max(xmax, ymax).bit_length() + 1  # room for the sign bit

    packed = format(bits, "05b")
    for value in (0, xmax, 0, ymax):
        packed += format(value, f"0{bits}b")
    packed += "0" * (-len(packed) % 8)

    return bytes(
        int(packed[at:at + 8], 2) for at in range(0, len(packed), 8))


def tag(code: int, body: bytes = b"") -> bytes:
    """A short tag; nothing here is anywhere near the 63-byte limit."""
    if len(body) >= 0x3F:
        raise ValueError("this writer only does short tags")
    return struct.pack("<H", (code << 6) | len(body)) + body


def build() -> bytes:
    body = b"".join((
        rect(STAGE_WIDTH, STAGE_HEIGHT),
        struct.pack("<H", FRAME_RATE << 8),  # frame rate, 8.8 fixed point
        struct.pack("<H", 1),                # one frame
        # ActionScript 3. Without this the movie is taken for AS2, where the
        # display classes the plugin creates by name do not exist.
        tag(TAG_FILE_ATTRIBUTES, struct.pack("<I", 1 << 3)),
        # Black, and left fully transparent by the plugin: the menu must not
        # dim the game behind it.
        tag(TAG_SET_BACKGROUND_COLOR, bytes((0, 0, 0))),
        tag(TAG_SHOW_FRAME),
        tag(TAG_END),
    ))

    # The length in the header counts the header itself.
    head = b"FWS" + bytes((9,))
    return head + struct.pack("<I", len(head) + 4 + len(body)) + body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    data = build()
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_bytes(data)
    print(f"wrote {TARGET} ({len(data)} bytes)")

    if arguments.check:
        print(f"  signature {data[:3].decode()}, version {data[3]}")
        print(f"  stage {STAGE_WIDTH}x{STAGE_HEIGHT} at {FRAME_RATE} fps")
    return 0


if __name__ == "__main__":
    sys.exit(main())
