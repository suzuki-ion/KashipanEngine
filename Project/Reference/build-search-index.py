# -*- coding: utf-8 -*-
"""Builds assets/search-index.js by scanning Engine/Editor/Script HTML pages.

Run this script (from anywhere) after adding, removing, or editing reference pages,
then commit the regenerated assets/search-index.js alongside the content change.

For each page, extracts:
  - title (h1 text)
  - headings (h2/h3 text, in document order) -> anchor ids "kehead-0", "kehead-1", ... are
    assigned at runtime by assets/nav.js's assignHeadingIds(), using the SAME document-order
    rule (querySelectorAll("main h2, main h3")), so the indices computed here must match exactly.
  - text blocks (p / li / td content), each tagged with the index of the nearest preceding
    heading (or -1 if before the first heading), for near-miss anchor jumping on body matches.
"""
import html
import json
import os
import re

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.join(ROOT, "assets", "search-index.js")

SITES = [
    ("Engine", "エンジン本体"),
    ("Editor", "エディタ"),
    ("Script", "スクリプト"),
]

TAG_RE = re.compile(r'<[^>]+>')
WS_RE = re.compile(r'\s+')

# (?=[\s>]) ensures the tag name isn't a prefix of a longer tag name
# (e.g. bare "<li[^>]*>" would also match "<link ...>").
BLOCK_RE = re.compile(
    r'<h2(?=[\s>])[^>]*>(?P<h2>.*?)</h2>'
    r'|<h3(?=[\s>])[^>]*>(?P<h3>.*?)</h3>'
    r'|<p(?=[\s>])[^>]*>(?P<p>.*?)</p>'
    r'|<li(?=[\s>])[^>]*>(?P<li>.*?)</li>'
    r'|<td(?=[\s>])[^>]*>(?P<td>.*?)</td>',
    re.DOTALL,
)

H1_RE = re.compile(r'<h1(?=[\s>])[^>]*>(.*?)</h1>', re.DOTALL)
PRE_RE = re.compile(r'<pre\b.*?</pre>', re.DOTALL)


def clean_text(raw):
    text = TAG_RE.sub(' ', raw)
    text = html.unescape(text)
    text = WS_RE.sub(' ', text).strip()
    return text


def build_page_entry(site, href, content):
    content_no_pre = PRE_RE.sub(' ', content)

    m = H1_RE.search(content_no_pre)
    title = clean_text(m.group(1)) if m else href

    headings = []
    blocks = []
    heading_index = -1

    for m in BLOCK_RE.finditer(content_no_pre):
        if m.group('h2') is not None:
            heading_index += 1
            headings.append(clean_text(m.group('h2')))
        elif m.group('h3') is not None:
            heading_index += 1
            headings.append(clean_text(m.group('h3')))
        else:
            raw = m.group('p') or m.group('li') or m.group('td')
            text = clean_text(raw)
            if len(text) < 3:
                continue
            blocks.append({"t": text, "h": heading_index})

    return {
        "site": site,
        "href": href.replace(os.sep, "/"),
        "title": title,
        "headings": headings,
        "blocks": blocks,
    }


def main():
    pages = []
    for site_dir, site_label in SITES:
        base = os.path.join(ROOT, site_dir)
        for dirpath, dirnames, filenames in os.walk(base):
            for fn in sorted(filenames):
                if not fn.endswith(".html"):
                    continue
                full = os.path.join(dirpath, fn)
                href = os.path.relpath(full, ROOT)
                with open(full, encoding="utf-8") as f:
                    content = f.read()
                pages.append(build_page_entry(site_dir, href, content))

    site_labels = {s: label for s, label in SITES}

    js = []
    js.append("// KashipanEngine Reference - unified search index (auto-generated)")
    js.append("// Regenerate with Reference/build-search-index.py when reference content changes.")
    js.append("const KE_SITE_LABELS = " + json.dumps(site_labels, ensure_ascii=False) + ";")
    js.append("const KE_SEARCH_INDEX = " + json.dumps(pages, ensure_ascii=False, separators=(",", ":")) + ";")
    out = "\n".join(js) + "\n"

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write(out)

    total_headings = sum(len(p["headings"]) for p in pages)
    total_blocks = sum(len(p["blocks"]) for p in pages)
    print(f"pages: {len(pages)}  headings: {total_headings}  blocks: {total_blocks}")
    print(f"output size: {os.path.getsize(OUT_PATH) / 1024:.1f} KB")


if __name__ == "__main__":
    main()
