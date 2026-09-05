"""Look at what Fallout 4 actually does at a given REL::ID.

Every wrong guess about the engine costs a test in the game, and twice now
it cost an item in the save. Reading the machine code is cheaper: the
Address Library says where an ID lives, the exe says what is there.

    py -3 tools/f4dis.py id 254434            # disassemble that function
    py -3 tools/f4dis.py id 254434 --count 80
    py -3 tools/f4dis.py rva 0x1234567        # same, by address
    py -3 tools/f4dis.py vtable 1064496       # the entries of a vtable
    py -3 tools/f4dis.py xref 1064496         # who mentions that address
    py -3 tools/f4dis.py peek dump.txt        # code the plugin copied out

Fallout4.exe is packed on disk (the .bind section is Steam's), so the code
section reads as noise from the file -- vtables and other data are fine. For
code, the plugin copies the bytes out of the running game with PeekIDs in the
INI, and `peek` disassembles what it wrote.

Addresses in the output carry their ID in brackets when the Address Library
knows one, so a call leads straight to the next thing to look up.

The database is Data\\F4SE\\Plugins\\version-1-10-163-0.bin, the same file
CommonLibF4 reads at runtime, and the format is the one the Address Library
itself documents.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

GAME = Path(r"G:\Program Files (x86)\Steam\steamapps\common\Fallout 4")
EXE = GAME / "Fallout4.exe"
DATABASE = GAME / "Data" / "F4SE" / "Plugins" / "version-1-10-163-0.bin"


def load_database(path: Path) -> dict[int, int]:
    """id -> offset from the image base.

    The Fallout 4 database is the plain kind: a count, then one pair of
    64-bit numbers per entry. No delta encoding like the later Skyrim files.
    """
    raw = path.read_bytes()
    count = struct.unpack_from("<Q", raw, 0)[0]
    if 8 + count * 16 != len(raw):
        raise SystemExit(
            f"{path.name} is not the expected shape "
            f"({count} entries do not fill {len(raw)} bytes)")

    numbers = struct.unpack_from(f"<{count * 2}Q", raw, 8)
    return dict(zip(numbers[0::2], numbers[1::2]))


class Image:
    """Just enough PE to turn an address into a file offset and back."""

    def __init__(self, path: Path) -> None:
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise SystemExit("not a PE image")
        sections = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.base = struct.unpack_from("<Q", self.data, pe + 24 + 24)[0]

        table = pe + 24 + optional_size
        self.sections = []
        for index in range(sections):
            entry = table + index * 40
            name = self.data[entry:entry + 8].rstrip(b"\0").decode("latin-1")
            virtual_size, rva, raw_size, raw = struct.unpack_from(
                "<IIII", self.data, entry + 8)
            self.sections.append((name, rva, max(virtual_size, raw_size), raw))

    def offset_of(self, rva: int) -> int | None:
        for _, start, size, raw in self.sections:
            if start <= rva < start + size:
                return raw + (rva - start)
        return None

    def read(self, rva: int, count: int) -> bytes:
        offset = self.offset_of(rva)
        if offset is None:
            return b""
        return self.data[offset:offset + count]


def format_address(rva: int, names: dict[int, int]) -> str:
    identifier = names.get(rva)
    return f"{rva:#x}" + (f" [ID {identifier}]" if identifier else "")


def disassemble(image: Image, rva: int, count: int, names: dict[int, int]) -> None:
    import capstone

    engine = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    engine.detail = False

    code = image.read(rva, max(count * 15, 64))
    shown = 0
    for instruction in engine.disasm(code, rva):
        target = ""
        if instruction.mnemonic in ("call", "jmp") and \
                instruction.op_str.startswith("0x"):
            target = "   -> " + format_address(
                int(instruction.op_str, 16), names)
        print(f"  {instruction.address:#010x}  {instruction.mnemonic:<7} "
              f"{instruction.op_str}{target}")
        shown += 1
        if shown >= count:
            break
        if instruction.mnemonic in ("ret", "int3"):
            break


def show_peek(path: Path, names: dict[int, int], count: int) -> None:
    """Disassemble the blocks the plugin copied out of the running game.

    Each block is a comment line naming what it is and where it came from,
    followed by its bytes in hex.
    """
    import capstone

    engine = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    state = {"label": "", "rva": 0, "hex": []}

    def flush() -> None:
        if not state["hex"]:
            return
        code = bytes.fromhex("".join(state["hex"]))
        rva = state["rva"]
        print()
        print(f"=== {state['label']} at {format_address(rva, names)} "
              f"({len(code)} bytes) ===")
        for shown, instruction in enumerate(engine.disasm(code, rva)):
            if shown >= count:
                print("  ...")
                break
            target = ""
            if instruction.mnemonic in ("call", "jmp"):
                if instruction.op_str.startswith("0x"):
                    target = "   -> " + format_address(
                        int(instruction.op_str, 16), names)
            print(f"  {instruction.address:#010x}  {instruction.mnemonic:<7} "
                  f"{instruction.op_str}{target}")

    for line in path.read_text().splitlines():
        if line.startswith("# base"):
            continue
        if line.startswith("#"):
            flush()
            head = line[1:].strip()
            state = {
                "label": head.split(" rva ")[0].strip(),
                "rva": int(head.split(" rva ")[1].split()[0], 16),
                "hex": [],
            }
        elif line.strip():
            state["hex"].append(line.strip())
    flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("what", choices=["id", "rva", "vtable", "xref", "peek"])
    parser.add_argument("value")
    parser.add_argument("--count", type=int, default=40)
    arguments = parser.parse_args()

    database = load_database(DATABASE)
    image = Image(EXE)
    names = {offset: identifier for identifier, offset in database.items()}

    if arguments.what == "peek":
        show_peek(Path(arguments.value), names, arguments.count)
        return 0

    def resolve(text: str) -> int:
        number = int(text, 0)
        if arguments.what == "rva":
            return number - image.base if number > image.base else number
        if number not in database:
            raise SystemExit(f"the database has no ID {number}")
        return database[number]

    if arguments.what in ("id", "rva"):
        rva = resolve(arguments.value)
        print(f"{arguments.value} -> {format_address(rva, names)}")
        disassemble(image, rva, arguments.count, names)
        return 0

    if arguments.what == "vtable":
        rva = resolve(arguments.value)
        print(f"vtable at {format_address(rva, names)}")
        raw = image.read(rva, 8 * arguments.count)
        for slot in range(len(raw) // 8):
            pointer = struct.unpack_from("<Q", raw, slot * 8)[0]
            if pointer < image.base:
                break
            print(f"  [{slot:2}] {format_address(pointer - image.base, names)}")
        return 0

    # xref: the exe is small enough to scan for the eight bytes of an
    # absolute address. That is how a vtable pointer is written into an
    # object, so it finds the constructors.
    rva = resolve(arguments.value)
    needle = struct.pack("<Q", image.base + rva)
    print(f"looking for {image.base + rva:#x} ({format_address(rva, names)})")
    found = 0
    at = image.data.find(needle)
    while at >= 0 and found < arguments.count:
        for name, start, size, raw in image.sections:
            if raw <= at < raw + size:
                print(f"  {start + (at - raw):#010x}  in {name}")
                found += 1
                break
        at = image.data.find(needle, at + 1)
    if not found:
        print("  nothing -- the address is probably loaded relative (lea)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
