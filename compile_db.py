#!/usr/bin/env python3
"""
compile_db.py — Compiles skylander_db.json into skylander_db.bin
for use with the Skylandex Flipper Zero app.

Binary format:
  Header (12 bytes):
    [uint32 magic]        — 0x4B445953 ("SKYD")
    [uint32 version]      — DB_VERSION
    [uint32 record_count]

  Record (52 bytes each, fixed-size):
    [uint16 character_id]
    [uint16 variant_id]
    [uint8  element_id]
    [uint8  padding]
    [char   name[30]]     — null-terminated ASCII
    [char   element[16]]  — null-terminated ASCII

Usage:
    python3 compile_db.py
    python3 compile_db.py --input skylander_db.json --output resources/skylander_db.bin
"""

import json
import struct
import argparse
import sys
import os

DB_VERSION = 1
MAGIC = 0x4B445953  # "SKYD"

ELEMENT_IDS = {
    "Unknown": 0, "Magic": 1, "Water": 2, "Earth": 3,
    "Fire": 4, "Air": 5, "Undead": 6, "Life": 7,
    "Tech": 8, "Dark": 9, "Light": 10,
}

ELEMENT_NAMES_BY_ID = {
    0: "Unknown", 1: "Magic", 2: "Water", 3: "Earth",
    4: "Fire", 5: "Air", 6: "Undead", 7: "Life",
    8: "Tech", 9: "Dark", 10: "Light",
}

MAX_NAME_LEN = 30
MAX_ELEMENT_LEN = 16


def parse_hex_int(value):
    if isinstance(value, int):
        return value
    s = str(value).strip()
    if s.startswith("0x") or s.startswith("0X"):
        return int(s, 16)
    return int(s, 10)


def encode_str(text, max_len):
    encoded = text.encode("utf-8", errors="replace")
    if len(encoded) >= max_len:
        encoded = encoded[:max_len - 1]
    return encoded.ljust(max_len, b"\x00")


def compile_db(input_path, output_path):
    print(f"Reading {input_path}...")
    with open(input_path, "r", encoding="utf-8") as f:
        records = json.load(f)

    if not isinstance(records, list):
        print("ERROR: JSON root must be an array of records.", file=sys.stderr)
        sys.exit(1)

    print(f"  {len(records)} records found")

    errors = []
    compiled = []
    element_errors = set()

    for i, rec in enumerate(records):
        try:
            char_id = parse_hex_int(rec["char_id"])
            variant_id = parse_hex_int(rec["variant_id"])
            element_id = int(rec["element"])
            name = str(rec["name"])
        except (KeyError, ValueError) as e:
            errors.append(f"  Record {i}: {e} — {rec}")
            continue

        if not (0 <= char_id <= 0xFFFF):
            errors.append(f"  Record {i}: char_id 0x{char_id:04X} out of range")
            continue
        if not (0 <= variant_id <= 0xFFFF):
            errors.append(f"  Record {i}: variant_id 0x{variant_id:04X} out of range")
            continue
        if element_id not in ELEMENT_NAMES_BY_ID:
            element_errors.add(element_id)
            errors.append(f"  Record {i}: element {element_id} is not a known element ID")
            continue

        element_name = ELEMENT_NAMES_BY_ID[element_id]

        name_packed = encode_str(name, MAX_NAME_LEN)
        element_packed = encode_str(element_name, MAX_ELEMENT_LEN)

        compiled.append((char_id, variant_id, element_id, name_packed, element_packed))

    if element_errors:
        print(f"\nUnknown element IDs encountered: {sorted(element_errors)}")
        print(f"Valid element IDs: 0=Unknown 1=Magic 2=Water 3=Earth 4=Fire 5=Air 6=Undead 7=Life 8=Tech 9=Dark 10=Light")

    if errors:
        print(f"\nERRORS ({len(errors)}):")
        for e in errors:
            print(e)
        print(f"\n{len(compiled)} records OK, {len(errors)} failed.")
        sys.exit(1)

    print(f"\nWriting {output_path}...")
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    with open(output_path, "wb") as f:
        f.write(struct.pack("<III", MAGIC, DB_VERSION, len(compiled)))

        for char_id, variant_id, element_id, name_bytes, element_bytes in compiled:
            f.write(struct.pack("<HHBB", char_id, variant_id, element_id, 0))
            f.write(name_bytes)
            f.write(element_bytes)

    file_size = os.path.getsize(output_path)
    expected = 12 + len(compiled) * 52
    print(f"Done!")
    print(f"  Records : {len(compiled)}")
    print(f"  Version : {DB_VERSION}")
    print(f"  Magic   : 0x{MAGIC:08X} (SKYD)")
    print(f"  Output  : {output_path} ({file_size} bytes, expected {expected})")
    assert file_size == expected, f"Size mismatch: got {file_size}, expected {expected}"
    print(f"  Size check: OK")


def main():
    parser = argparse.ArgumentParser(
        description="Compile skylander_db.json → skylander_db.bin"
    )
    parser.add_argument(
        "--input", "-i",
        default=os.path.join(os.path.dirname(__file__), "skylander_db.json"),
        help="Path to input JSON",
    )
    parser.add_argument(
        "--output", "-o",
        default=os.path.join(os.path.dirname(__file__), "resources", "skylander_db.bin"),
        help="Path to output binary",
    )
    args = parser.parse_args()

    compile_db(args.input, args.output)

    print(f"\nRemember: bump DB_VERSION in compile_db.py AND character_db.c")
    print(f"when you change the data or binary format.")


if __name__ == "__main__":
    main()
