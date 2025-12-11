#!/usr/bin/env python3
"""
Generate a snapshot schema header by scanning Snapshot::xfer implementations.

This is a lightweight, dependency-free parser that looks for:
  void ClassName::xfer( Xfer *xferVar )
and records xferVar->xferFoo(&field) calls in source order.

Output: a header with SnapshotSchemaField arrays describing the observed order.

Notes:
- This is a best-effort heuristic parser (no full C++ parsing). It works on the
  existing codebase patterns: direct calls on the Xfer pointer inside xfer().
- Offsets are accumulated only while fixed-size fields are seen; variable-size
  calls mark the offset as unknown and subsequent offsets stay unknown.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
from typing import Dict, Iterable, List, Optional, Tuple


TYPE_MAP: Dict[str, Tuple[str, Optional[int]]] = {
    "xferByte": ("Byte", 1),
    "xferUnsignedByte": ("UnsignedByte", 1),
    "xferVersion": ("UnsignedByte", 1),
    "xferBool": ("Bool", 1),
    "xferShort": ("Short", 2),
    "xferUnsignedShort": ("UnsignedShort", 2),
    "xferInt": ("Int", 4),
    "xferUnsignedInt": ("UnsignedInt", 4),
    "xferInt64": ("Int64", 8),
    "xferReal": ("Real", 4),
    "xferCoord2D": ("Coord2D", 8),
    "xferCoord3D": ("Coord3D", 12),
    "xferICoord2D": ("ICoord2D", 8),
    "xferICoord3D": ("ICoord3D", 12),
    "xferRGBColor": ("RGBColor", 12),
    "xferRGBAColorInt": ("RGBAColorInt", 16),
    "xferRGBAColorReal": ("RGBAColorReal", 16),
    "xferAsciiString": ("AsciiString", None),
    "xferUnicodeString": ("UnicodeString", None),
    "xferMarkerLabel": ("MarkerLabel", None),
    "xferUser": ("UserData", None),
    "beginBlock": ("BlockSize", 4),
    # endBlock rewrites the size for the most recent beginBlock; no new bytes.
    # We keep it to preserve call order but mark as size 0.
    "endBlock": ("EndBlock", 0),
}


CALL_SIG_RE = re.compile(
    r"void\s+(?P<class>[A-Za-z_]\w*)\s*::\s*xfer\s*\(\s*Xfer\s*\*\s*(?P<xfervar>\w+)\s*\)",
    re.MULTILINE,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Snapshot schema header.")
    parser.add_argument(
        "--output",
        required=True,
        help="Path to write GeneratedSnapshotSchema.h",
    )
    parser.add_argument(
        "--source-root",
        action="append",
        default=[],
        help="Root directory to scan for Snapshot::xfer implementations (repeatable).",
    )
    parser.add_argument(
        "--compile-commands",
        default=None,
        help="Optional compile_commands.json to seed source list.",
    )
    return parser.parse_args()


def load_compile_commands(path: str) -> List[pathlib.Path]:
    try:
        data = json.loads(pathlib.Path(path).read_text())
    except Exception:
        return []
    files: List[pathlib.Path] = []
    for entry in data:
        file_path = entry.get("file") or entry.get("directory")
        if not file_path:
            continue
        files.append(pathlib.Path(entry["file"]).resolve())
    return files


def strip_comments_and_strings(src: str) -> str:
    out = []
    i = 0
    n = len(src)
    state = "code"
    while i < n:
        ch = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            if ch == '"':
                state = "string"
                out.append(" ")
                i += 1
                continue
            if ch == "'":
                state = "char"
                out.append(" ")
                i += 1
                continue
            out.append(ch)
            i += 1
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
            out.append(" ")
            i += 1
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                out.append("  ")
                i += 2
                continue
            out.append(" ")
            i += 1
        elif state == "string":
            if ch == "\\":
                out.append("  ")
                i += 2
                continue
            if ch == '"':
                state = "code"
            out.append(" ")
            i += 1
        elif state == "char":
            if ch == "\\":
                out.append("  ")
                i += 2
                continue
            if ch == "'":
                state = "code"
            out.append(" ")
            i += 1
    return "".join(out)


def find_function_body(text: str, start_idx: int) -> Tuple[str, int]:
    brace_start = text.find("{", start_idx)
    if brace_start == -1:
        return "", -1
    depth = 0
    i = brace_start
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace_start + 1 : i], brace_start + 1
        i += 1
    return "", -1


def normalize_field_name(arg_expr: str) -> str:
    cleaned = arg_expr.strip()
    cleaned = cleaned.lstrip("&*")
    cleaned = cleaned.strip()
    cleaned = cleaned.strip("()")
    cleaned = cleaned.replace("->", ".")
    return cleaned or "unnamed"


def iter_xfer_calls(body: str, xfer_var: str) -> Iterable[Tuple[str, str, int]]:
    call_re = re.compile(
        rf"\b{xfer_var}\b\s*->\s*(?P<method>\w+)\s*\((?P<args>[^;]*?)\)\s*;",
        re.S,
    )
    for m in call_re.finditer(body):
        yield m.group("method"), m.group("args").strip(), m.start()


def gather_from_file(path: pathlib.Path) -> List[Dict]:
    text = path.read_text(errors="ignore")
    stripped = strip_comments_and_strings(text)
    results: List[Dict] = []
    for sig in CALL_SIG_RE.finditer(stripped):
        cls = sig.group("class")
        xfer_var = sig.group("xfervar")
        body, body_start = find_function_body(stripped, sig.end())
        if not body:
            continue
        offset_known = True
        offset = 0
        for method, args, _ in iter_xfer_calls(body, xfer_var):
            mapped = TYPE_MAP.get(method, ("Unknown", None))
            size = mapped[1]
            field_name = normalize_field_name(args.split(",")[0])
            results.append(
                {
                    "class": cls,
                    "type": mapped[0],
                    "size": size,
                    "offset": offset if offset_known and size is not None else -1,
                    "field": field_name,
                }
            )
            if offset_known and size is not None:
                offset += size
            else:
                offset_known = False
    return results


def discover_sources(roots: List[str], compile_commands: Optional[str]) -> List[pathlib.Path]:
    files: List[pathlib.Path] = []
    seen = set()
    if compile_commands and pathlib.Path(compile_commands).exists():
        for f in load_compile_commands(compile_commands):
            if f.suffix in (".cpp", ".cc") and f not in seen:
                seen.add(f)
                files.append(f)
    for root in roots or ["."]:
        for dirpath, _, filenames in os.walk(root):
            for fn in filenames:
                if fn.endswith((".cpp", ".cc")):
                    p = pathlib.Path(dirpath) / fn
                    if p not in seen:
                        seen.add(p)
                        files.append(p)
    return files


def write_header(output: pathlib.Path, records: List[Dict]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    by_class: Dict[str, List[Dict]] = {}
    for rec in records:
        by_class.setdefault(rec["class"], []).append(rec)
    with output.open("w", encoding="utf-8") as f:
        f.write("// Auto-generated by generate_snapshot_schema.py. Do not edit.\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n\n")
        f.write("struct SnapshotSchemaField {\n")
        f.write("    const char* name;\n")
        f.write("    const char* type;\n")
        f.write("    int64_t offset; // -1 when unknown/variable after this point\n")
        f.write("    int64_t size;   // -1 for variable length, 0 for endBlock\n")
        f.write("};\n\n")
        f.write("namespace SnapshotSchema {\n")
        for cls, fields in sorted(by_class.items()):
            f.write(f"static constexpr SnapshotSchemaField {cls}[] = {{\n")
            for field in fields:
                off = field["offset"]
                size = field["size"] if field["size"] is not None else -1
                f.write(f'    {{"{field["field"]}", "{field["type"]}", {off}, {size}}},\n')
            f.write("};\n\n")
        f.write("} // namespace SnapshotSchema\n")


def main() -> int:
    args = parse_args()
    sources = discover_sources(args.source_root, args.compile_commands)
    all_records: List[Dict] = []
    for path in sources:
        if not path.exists():
            continue
        try:
            recs = gather_from_file(path)
        except Exception:
            continue
        all_records.extend(recs)
    if not all_records:
        sys.stderr.write("No Snapshot::xfer implementations found; nothing generated.\n")
    write_header(pathlib.Path(args.output), all_records)
    return 0


if __name__ == "__main__":
    sys.exit(main())
