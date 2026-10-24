#!/usr/bin/env python3
"""Compare rendering test images with those of a base commit and write an HTML report.

Usage: compare-rendering.py GAME RESULT_DIR BASE_DIR BASE_SHA REPORT_HTML

Every *.bmp in RESULT_DIR is compared with the same file in BASE_DIR. Differences
become GitHub warnings and a job summary table; they never fail the job, because
an intended rendering change is accepted by reviewing the report and merging.
"""
import base64
import html
import os
import struct
import sys
import zlib
from pathlib import Path


def read_bmp(path):
    """Read a 32 bit BI_RGB BMP as (width, height, rows of RGB bytes)."""
    data = path.read_bytes()
    offset, = struct.unpack_from('<I', data, 10)
    width, height, _, bits = struct.unpack_from('<iiHH', data, 18)
    if bits != 32:
        raise ValueError(f'{path}: expected a 32 bit BMP, got {bits} bit')
    rows = []
    for y in range(abs(height)):
        bgrx = data[offset + y * width * 4:offset + (y + 1) * width * 4]
        rgb = bytearray(width * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = bgrx[2::4], bgrx[1::4], bgrx[0::4]
        rows.append(bytes(rgb))
    if height > 0:
        rows.reverse()  # bottom-up rows
    return width, abs(height), rows


def png(width, height, rows):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    raw = b''.join(b'\0' + row for row in rows)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def diff(result, base):
    """Return (differing pixels, largest channel difference, diff image rows).

    The diff image marks differing pixels magenta over a dimmed copy of the result.
    """
    count = peak = 0
    rows = []
    for a, b in zip(result, base):
        out = bytearray(len(a))
        for i in range(0, len(a), 3):
            d = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
            if d:
                count += 1
                peak = max(peak, d)
                out[i:i + 3] = b'\xff\x00\xff'
            else:
                out[i:i + 3] = bytes([(a[i] + a[i + 1] + a[i + 2]) // 12 + 32] * 3)
        rows.append(bytes(out))
    return count, peak, rows


def data_uri(image):
    return 'data:image/png;base64,' + base64.b64encode(png(*image)).decode()


def compare(result_dir, base_dir, has_base):
    entries = []
    names = sorted({p.name for p in result_dir.glob('*.bmp')} | {p.name for p in base_dir.glob('*.bmp')})
    for name in names:
        result = read_bmp(result_dir / name) if (result_dir / name).exists() else None
        base = read_bmp(base_dir / name) if (base_dir / name).exists() else None
        entry = {'name': name, 'result': result, 'base': base, 'diff': None, 'pixels': '', 'peak': ''}
        if not has_base:
            entry['status'] = 'not compared'
        elif result is None:
            entry['status'] = 'removed'
        elif base is None:
            entry['status'] = 'new'
        elif result[:2] != base[:2]:
            entry['status'] = 'size changed'
        else:
            count, peak, rows = diff(result[2], base[2])
            entry.update(status='changed' if count else 'unchanged', pixels=count, peak=peak)
            if count:
                entry['diff'] = (result[0], result[1], rows)
        entries.append(entry)
    order = ['changed', 'size changed', 'removed', 'new', 'not compared', 'unchanged']
    return sorted(entries, key=lambda e: (order.index(e['status']), e['name']))


PAGE = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{game} Rendering Report</title>
<style>
:root {{ --bg: #ffffff; --fg: #1f2328; --muted: #59636e; --line: #d1d9e0; --changed: #bc4c00; --ok: #1a7f37; }}
@media (prefers-color-scheme: dark) {{
  :root {{ --bg: #0d1117; --fg: #e6edf3; --muted: #9198a1; --line: #3d444d; --changed: #f0883e; --ok: #3fb950; }}
}}
body {{ margin: 0 auto; max-width: 1100px; padding: 16px; background: var(--bg); color: var(--fg); font: 14px/1.5 system-ui, sans-serif; }}
h1 {{ font-size: 20px; margin: 0 0 4px; }}
.meta {{ color: var(--muted); margin-bottom: 16px; }}
section {{ border-top: 1px solid var(--line); padding: 12px 0; }}
h2 {{ font-size: 16px; margin: 0 0 8px; }}
.status {{ font-weight: 600; color: var(--muted); }}
.status.changed, .status.size-changed, .status.removed {{ color: var(--changed); }}
.status.unchanged {{ color: var(--ok); }}
.images {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(240px, 1fr)); gap: 12px; }}
figure {{ margin: 0; }}
figcaption {{ color: var(--muted); font-size: 12px; }}
img {{ display: block; width: 100%; height: auto; box-sizing: border-box; image-rendering: pixelated; border: 1px solid var(--line); }}
img.flip {{ cursor: pointer; }}
</style>
</head>
<body>
<h1>{game} rendering</h1>
<div class="meta">{meta}</div>
{sections}
<script>
// Hold the mouse on a result image to show the base image in its place.
for (const img of document.querySelectorAll('img.flip')) {{
  const result = img.src;
  img.addEventListener('pointerdown', () => img.src = img.dataset.base);
  for (const event of ['pointerup', 'pointerleave']) img.addEventListener(event, () => img.src = result);
}}
</script>
</body>
</html>
'''


def figure(caption, image, base=None):
    flip = f' class="flip" data-base="{data_uri(base)}"' if base else ''
    return (f'<figure><img src="{data_uri(image)}" alt="{caption}"{flip}>'
            f'<figcaption>{caption}</figcaption></figure>')


def section(entry):
    stats = f', {entry["pixels"]} pixels differ, largest channel difference {entry["peak"]}' if entry['pixels'] else ''
    figures = []
    if entry['base']:
        figures.append(figure('Base', entry['base']))
    if entry['result']:
        same_size = entry['base'] and entry['base'][:2] == entry['result'][:2]
        figures.append(figure('Result (hold to show base)' if same_size else 'Result', entry['result'],
                              entry['base'] if same_size else None))
    if entry['diff']:
        figures.append(figure('Differences', entry['diff']))
    css = entry['status'].replace(' ', '-')
    return (f'<section><h2>{html.escape(entry["name"])}</h2>'
            f'<div class="status {css}">{entry["status"]}{stats}</div>'
            f'<div class="images">{"".join(figures)}</div></section>')


def main():
    game, result_dir, base_dir, base_sha, report = sys.argv[1:6]
    result_dir, base_dir = Path(result_dir), Path(base_dir)
    entries = compare(result_dir, base_dir, bool(base_sha))
    meta = (f'Compared with the base commit {html.escape(base_sha[:10])}.' if base_sha else
            'No base rendering was found, so the images are shown without a comparison.')
    Path(report).write_text(PAGE.format(game=html.escape(game), meta=meta,
                                        sections='\n'.join(section(e) for e in entries)), encoding='utf-8')

    lines = [f'### {game} Rendering', '', meta, '',
             '| Image | Status | Differing pixels | Largest channel difference |', '| --- | --- | --- | --- |']
    lines += [f'| {e["name"]} | {e["status"]} | {e["pixels"]} | {e["peak"]} |' for e in entries]
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a', encoding='utf-8') as file:
            file.write('\n'.join(lines) + '\n\n')
    else:
        print('\n'.join(lines))
    for e in entries:
        if e['status'] in ('changed', 'size changed', 'removed'):
            print(f'::warning title=Rendering {e["status"]}::{game} {e["name"]} is {e["status"]} '
                  f'compared with base {base_sha[:10]}. See the rendering report.')


if __name__ == '__main__':
    main()
