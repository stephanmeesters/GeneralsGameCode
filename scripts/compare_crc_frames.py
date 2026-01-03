#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


CRC_FILENAME_RE = re.compile(r"^crc_frame_(\d+)\.txt$")


def parse_final_crc(path: Path) -> str:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return ""
    # Find last non-empty line
    for line in reversed(lines):
        line = line.strip()
        if line:
            return line
    return ""


def extract_crc_value(line: str) -> str:
    # Expect "FinalCRC: 0xDEADBEEF" but fall back to full line for diffing.
    if line.startswith("FinalCRC:"):
        return line.split(":", 1)[1].strip()
    return line


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare crc_frame_XXXX.txt files between two directories."
    )
    parser.add_argument("dir_a", type=Path, help="First directory")
    parser.add_argument("dir_b", type=Path, help="Second directory")
    args = parser.parse_args()

    if not args.dir_a.is_dir():
        print(f"Not a directory: {args.dir_a}")
        return 2
    if not args.dir_b.is_dir():
        print(f"Not a directory: {args.dir_b}")
        return 2

    files_a = {}
    for path in args.dir_a.iterdir():
        match = CRC_FILENAME_RE.match(path.name)
        if match:
            files_a[path.name] = int(match.group(1))

    files_b = {p.name for p in args.dir_b.iterdir() if CRC_FILENAME_RE.match(p.name)}
    common = [name for name in files_a.keys() if name in files_b]
    common.sort(key=lambda name: files_a[name])

    if not common:
        print("No matching crc_frame_*.txt files found.")
        return 1

    for name in common:
        path_a = args.dir_a / name
        path_b = args.dir_b / name
        line_a = parse_final_crc(path_a)
        line_b = parse_final_crc(path_b)
        crc_a = extract_crc_value(line_a)
        crc_b = extract_crc_value(line_b)
        if crc_a != crc_b:
            print("Mismatch found:")
            print(f"  file: {name}")
            print(f"  {args.dir_a}: {line_a}")
            print(f"  {args.dir_b}: {line_b}")
            return 0

    print("No mismatches found.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
