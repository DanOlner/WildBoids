#!/usr/bin/env python3
"""Generate docs/champion_comparisons/index.html — a landing page linking every
champion-comparison report in that folder.

Scans for champion_comparison_*.html files (produced by compare_champions.py),
pulls the title / generated date / champion files out of the top of each, and
writes an index.html that links them. Standard-library only, so it runs in CI
with no pip install.

Usage:
    python scripts/build_comparison_index.py
    python scripts/build_comparison_index.py --dir docs/champion_comparisons
"""

import argparse
import html
import re
from datetime import date
from pathlib import Path

# The <title> and meta line live in the first ~2 KB, before any base64 image,
# so we only need to read the head of each (multi-MB) report.
HEAD_BYTES = 16384

REPORT_GLOB = 'champion_comparison_*.html'
DATE_RE = re.compile(r'^\d{4}-\d{2}-\d{2}')


def _search(pattern: str, text: str) -> str | None:
    m = re.search(pattern, text, re.DOTALL)
    return m.group(1).strip() if m else None


def extract_meta(path: Path) -> dict:
    """Pull display metadata out of a single comparison report."""
    with open(path, encoding='utf-8', errors='replace') as f:
        head = f.read(HEAD_BYTES)

    title = _search(r'<title>(.*?)</title>', head) or path.stem
    generated = _search(r'Generated:\s*(\d{4}-\d{2}-\d{2})', head) or ''
    source = _search(r'Source:\s*<code>(.*?)</code>', head) or ''

    # Champion files are listed as <code>…</code> after "Files:" in the meta line.
    files: list[str] = []
    if 'Files:' in head:
        tail = head.split('Files:', 1)[1].split('</p>', 1)[0]
        files = re.findall(r'<code>(.*?)</code>', tail)

    return {
        'title': html.unescape(title),
        'generated': generated,
        'source': html.unescape(source) or path.stem,
        'files': [html.unescape(x) for x in files],
        'size_mb': path.stat().st_size / (1024 * 1024),
        'href': path.name,
    }


def collect_reports(directory: Path) -> list[dict]:
    reports = [extract_meta(p) for p in sorted(directory.glob(REPORT_GLOB))]
    # Dated packages first, newest first; anything else alphabetically after.
    dated = sorted((r for r in reports if DATE_RE.match(r['source'])),
                   key=lambda r: r['source'], reverse=True)
    other = sorted((r for r in reports if not DATE_RE.match(r['source'])),
                   key=lambda r: r['source'])
    return dated + other


def render_card(r: dict) -> str:
    files_html = ''
    if r['files']:
        files_html = ('<ul class="files">'
                      + ''.join(f'<li>{html.escape(f)}</li>' for f in r['files'])
                      + '</ul>')
    generated = f'<span class="date">{html.escape(r["generated"])}</span>' if r['generated'] else ''
    return f'''      <a class="card" href="{html.escape(r['href'])}">
        <h2>{html.escape(r['source'])}</h2>
        <p class="meta">{generated}<span class="size">{r['size_mb']:.1f} MB</span></p>
        {files_html}
      </a>'''


def render_index(reports: list[dict]) -> str:
    if reports:
        cards = '\n'.join(render_card(r) for r in reports)
        body = f'<div class="grid">\n{cards}\n    </div>'
        count = f'{len(reports)} report{"s" if len(reports) != 1 else ""}'
    else:
        body = ('<p class="empty">No comparison reports yet. Generate one with '
                '<code>python scripts/compare_champions.py --package '
                'data/champion_packages/&lt;name&gt;</code>.</p>')
        count = '0 reports'

    return f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Wildboids — Champion Comparisons</title>
<style>
  :root {{
    --bg: #fafafa; --fg: #1a1a1a; --muted: #666; --card: #fff;
    --border: #ddd; --accent: #2166ac; --shadow: rgba(0,0,0,0.08);
  }}
  @media (prefers-color-scheme: dark) {{
    :root {{
      --bg: #16181c; --fg: #e6e6e6; --muted: #9aa0a6; --card: #1f2228;
      --border: #33373e; --accent: #6ea8ff; --shadow: rgba(0,0,0,0.4);
    }}
  }}
  * {{ box-sizing: border-box; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
         max-width: 1100px; margin: 0 auto; padding: 32px 20px;
         background: var(--bg); color: var(--fg); line-height: 1.5; }}
  h1 {{ margin: 0 0 4px; font-size: 1.8rem; }}
  .lede {{ color: var(--muted); margin: 0 0 28px; }}
  .grid {{ display: grid; gap: 16px;
          grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); }}
  .card {{ display: block; text-decoration: none; color: inherit;
          background: var(--card); border: 1px solid var(--border);
          border-radius: 10px; padding: 16px 18px;
          box-shadow: 0 1px 3px var(--shadow); transition: transform .08s, box-shadow .08s; }}
  .card:hover {{ transform: translateY(-2px); box-shadow: 0 4px 14px var(--shadow);
               border-color: var(--accent); }}
  .card h2 {{ margin: 0 0 6px; font-size: 1.15rem; color: var(--accent);
            word-break: break-word; }}
  .card .meta {{ margin: 0 0 8px; font-size: .82rem; color: var(--muted);
               display: flex; gap: 10px; }}
  .card .size {{ margin-left: auto; }}
  ul.files {{ margin: 0; padding-left: 18px; font-size: .82rem; color: var(--muted); }}
  ul.files li {{ word-break: break-all; }}
  .empty {{ color: var(--muted); }}
  code {{ background: var(--card); border: 1px solid var(--border);
         padding: 1px 5px; border-radius: 4px; font-size: .85em; }}
  footer {{ margin-top: 36px; color: var(--muted); font-size: .8rem;
           border-top: 1px solid var(--border); padding-top: 14px; }}
</style>
</head>
<body>
  <h1>Champion Comparisons</h1>
  <p class="lede">Evolved-brain and sensor-layout reports for Wildboids champions.
    Each links a matched prey + predator matchup. {count}.</p>
    {body}
  <footer>Auto-generated by <code>scripts/build_comparison_index.py</code>
    · last built {date.today()}</footer>
</body>
</html>
'''


def main():
    parser = argparse.ArgumentParser(
        description='Build the champion-comparison index.html landing page.')
    parser.add_argument('--dir', default='docs/champion_comparisons', metavar='DIR',
                        help='Folder holding champion_comparison_*.html reports '
                             '(default: docs/champion_comparisons).')
    parser.add_argument('--output', '-o', default=None, metavar='PATH',
                        help='Output path (default: <dir>/index.html).')
    args = parser.parse_args()

    directory = Path(args.dir)
    if not directory.is_dir():
        parser.error(f'not a directory: {directory}')

    reports = collect_reports(directory)
    out = Path(args.output) if args.output else directory / 'index.html'
    out.write_text(render_index(reports), encoding='utf-8')

    print(f'Wrote {out} ({len(reports)} report(s))')
    for r in reports:
        print(f'  - {r["source"]:40} {r["href"]}')


if __name__ == '__main__':
    main()
