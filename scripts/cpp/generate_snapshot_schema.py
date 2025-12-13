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
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
from typing import Dict, Iterable, List, Optional, Set, Tuple

ENUM_RE = re.compile(
    r"\benum\s+(?:class|struct)?\s*(?P<name>[A-Za-z_]\w*)\s*(?::\s*(?P<underlying>[A-Za-z_]\w*))?",
    re.MULTILINE,
)


def normalize_underlying(typ: str) -> str:
    t = typ.replace(" ", "").replace("\t", "")
    t = t.replace("unsigned", "u").lower()
    if t in ("char", "int8", "schar"):
        return "Byte"
    if t in ("uchar", "uint8"):
        return "UnsignedByte"
    if t in ("short", "int16"):
        return "Short"
    if t in ("ushort", "uint16"):
        return "UnsignedShort"
    if t in ("int", "long", "int32"):
        return "Int"
    if t in ("uint", "ulong", "uint32"):
        return "UnsignedInt"
    if t in ("longlong", "int64", "llong"):
        return "Int64"
    if t in ("ulonglong", "uint64"):
        return "Int64"
    return typ

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


def resolve_type(method: str) -> Tuple[str, Optional[int]]:
    if method in TYPE_MAP:
        return TYPE_MAP[method]
    if method.startswith("xfer") and len(method) > 4:
        return method[4:], None
    return method, None


def strip_comments(src: str) -> str:
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
    return "".join(out)


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
    parser.add_argument(
        "--xfer-header",
        default="Core/GameEngine/Include/Common/Xfer.h",
        help="Path to Xfer.h to limit parsing to xfer methods.",
    )
    parser.add_argument(
        "--gamestate",
        default="GeneralsMD/Code/GameEngine/Source/Common/System/SaveGame/GameState.cpp",
        help="Path to GameState.cpp to infer snapshot block mapping.",
    )
    parser.add_argument(
        "--enum-root",
        action="append",
        default=[],
        help="Root directory to scan for enums to improve xferUser typing (repeatable).",
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


def load_allowed_methods(xfer_header: Optional[str]) -> Optional[Set[str]]:
    if not xfer_header:
        return None
    header_path = pathlib.Path(xfer_header)
    if not header_path.exists():
        return None
    try:
        text = header_path.read_text(errors="ignore")
    except Exception:
        return None
    method_re = re.compile(
        r"\bvirtual\s+[A-Za-z_][\w:<>]*\s+(?P<name>(?:xfer\w+|beginBlock|endBlock))\s*\("
    )
    methods = {m.group("name") for m in method_re.finditer(text)}
    return methods or None


def gather_from_file(path: pathlib.Path, allowed_methods: Optional[Set[str]], enum_types: Dict[str, str]) -> List[Dict]:
    text = path.read_text(errors="ignore")
    stripped = strip_comments_and_strings(text)
    results: List[Dict] = []
    for sig in CALL_SIG_RE.finditer(stripped):
        cls = sig.group("class")
        xfer_var = sig.group("xfervar")
        body, body_start = find_function_body(stripped, sig.end())
        if not body:
            continue
        seen: Set[Tuple[str, str]] = set()
        for method, args, _ in iter_xfer_calls(body, xfer_var):
            if allowed_methods is not None and method not in allowed_methods:
                continue
            mapped = resolve_type(method)
            field_name = normalize_field_name(args.split(",")[0])
            if method == "xferUser":
                parts = [a.strip() for a in args.split(",")]
                if len(parts) >= 2:
                    size_expr = parts[1]
                    size_expr = size_expr.replace(" ", "")
                    m_sizeof = re.match(r"sizeof\((?P<type>[A-Za-z_]\w*)\)", size_expr)
                    if m_sizeof:
                        enum_name = m_sizeof.group("type")
                        if enum_name in enum_types:
                            mapped_type = normalize_underlying(enum_types[enum_name])
                            mapped = (mapped_type, None)
                    elif size_expr.isdigit():
                        literal = int(size_expr)
                        if literal == 1:
                            mapped = ("Byte", 1)
                        elif literal == 2:
                            mapped = ("Short", 2)
                        elif literal == 4:
                            mapped = ("Int", 4)
                        elif literal == 8:
                            mapped = ("Int64", 8)
            key = (field_name, mapped[0])
            if key in seen:
                continue
            seen.add(key)
            results.append(
                {
                    "class": cls,
                    "type": mapped[0],
                    "field": field_name,
                }
            )
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


def normalize_snapshot_name(expr: str, known_classes: Set[str]) -> Optional[str]:
    name = expr.strip()
    name = name.lstrip("&*")
    # discard any member access
    name = re.split(r"[.\-+>]", name)[0]
    if name.startswith("The") and len(name) > 3:
        candidate = name[3:]
        if candidate in known_classes:
            return candidate
        name = candidate
    if name in known_classes:
        return name
    return name if name else None


def parse_snapshot_blocks(game_state_path: str, known_classes: Set[str]) -> List[Tuple[str, str]]:
    path = pathlib.Path(game_state_path)
    if not path.exists():
        return []
    text = path.read_text(errors="ignore")
    defines = {m.group("name"): m.group("value") for m in re.finditer(r'#define\s+(?P<name>\w+)\s+"(?P<value>[^"]*)"', text)}
    stripped_comments = strip_comments(text)
    sig = re.search(r"void\s+GameState\s*::\s*init\s*\([^)]*\)", stripped_comments)
    if not sig:
        return []
    body, _ = find_function_body(stripped_comments, sig.end())
    block_calls: Dict[str, str] = {}
    call_re = re.compile(
        r"addSnapshotBlock\s*\(\s*(?P<block>[^,]+?)\s*,\s*(?P<snapshot>[^,]+?)\s*,",
        re.S,
    )
    for m in call_re.finditer(body):
        block_expr = m.group("block").strip()
        snapshot_expr = m.group("snapshot").strip()
        block = block_expr.strip('"')
        block = defines.get(block, block)
        snap_name = normalize_snapshot_name(snapshot_expr, known_classes)
        if snap_name in known_classes:
            block_calls.setdefault(block, snap_name)
    return list(block_calls.items())


def discover_enums(roots: List[str], compile_commands: Optional[str]) -> Dict[str, str]:
    enum_types: Dict[str, str] = {}
    files: List[pathlib.Path] = []
    seen = set()
    # reuse compile_commands to seed enum scan if provided
    if compile_commands and pathlib.Path(compile_commands).exists():
        for f in load_compile_commands(compile_commands):
            if f.suffix in (".cpp", ".cc", ".h", ".hpp", ".inl") and f not in seen:
                seen.add(f)
                files.append(f)
    for root in roots or ["."]:
        for dirpath, _, filenames in os.walk(root):
            for fn in filenames:
                if fn.endswith((".h", ".hpp", ".inl", ".cpp", ".cc")):
                    p = pathlib.Path(dirpath) / fn
                    if p not in seen:
                        seen.add(p)
                        files.append(p)
    for path in files:
        try:
            text = pathlib.Path(path).read_text(errors="ignore")
        except Exception:
            continue
        for m in ENUM_RE.finditer(text):
            name = m.group("name")
            underlying = m.group("underlying") or "Int"
            enum_types.setdefault(name, underlying)
    return enum_types


def write_header(output: pathlib.Path, records: List[Dict], block_mappings: List[Tuple[str, str]]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    by_class: Dict[str, List[Dict]] = {}
    for rec in records:
        by_class.setdefault(rec["class"], []).append(rec)
    classes = sorted(by_class.items())
    with output.open("w", encoding="utf-8") as f:
        f.write("// Auto-generated by generate_snapshot_schema.py. Do not edit.\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n")
        f.write("#include <span>\n")
        f.write("#include <string_view>\n")
        f.write("#include <unordered_map>\n\n")
        f.write("struct SnapshotSchemaField {\n")
        f.write("    const char* name;\n")
        f.write("    const char* type;\n")
        f.write("};\n\n")
        f.write("using SnapshotSchemaView = std::span<const SnapshotSchemaField>;\n\n")
        f.write("namespace SnapshotSchema {\n")
        for cls, fields in classes:
            f.write(f"static constexpr SnapshotSchemaField {cls}[] = {{\n")
            for field in fields:
                f.write(f'    {{"{field["field"]}", "{field["type"]}"}},\n')
            f.write("};\n\n")
        f.write("// Lookup: class name -> span over schema fields\n")
        f.write("static const inline std::unordered_map<std::string_view, SnapshotSchemaView> SCHEMAS = {\n")
        for cls, fields in classes:
            f.write('    {"%s", SnapshotSchemaView{%s}},\n' % (cls, cls))
        f.write("};\n\n")
        if block_mappings:
            f.write("// Lookup: block name -> schema view (from GameState::init)\n")
            f.write("static const inline std::unordered_map<std::string_view, SnapshotSchemaView> SNAPSHOT_BLOCK_SCHEMAS = {\n")
            for block, cls in block_mappings:
                f.write('    {"%s", SnapshotSchemaView{%s}},\n' % (block, cls))
            f.write("};\n\n")
        f.write("} // namespace SnapshotSchema\n")


def main() -> int:
    args = parse_args()
    allowed_methods = load_allowed_methods(args.xfer_header)
    enum_types = discover_enums(args.enum_root, args.compile_commands)
    sources = discover_sources(args.source_root, args.compile_commands)
    all_records: List[Dict] = []
    for path in sources:
        if not path.exists():
            continue
        try:
            recs = gather_from_file(path, allowed_methods, enum_types)
        except Exception:
            continue
        all_records.extend(recs)
    known_classes = {rec["class"] for rec in all_records}
    block_mappings = parse_snapshot_blocks(args.gamestate, known_classes)
    if not all_records:
        sys.stderr.write("No Snapshot::xfer implementations found; nothing generated.\n")
    write_header(pathlib.Path(args.output), all_records, block_mappings)
    return 0


if __name__ == "__main__":
    sys.exit(main())
