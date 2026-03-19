from __future__ import annotations

import argparse
import html
import os
from pathlib import Path
import re


CPP_STRING = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CPP_TOKEN = re.compile(
    r"(?P<num>\b\d+(?:\.\d+)?(?:[uUlLfF]{0,3})?\b)|"
    r"(?P<type>\b(?:void|bool|char|wchar_t|char16_t|char32_t|short|int|long|float|double|signed|unsigned|size_t|int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t)\b)|"
    r"(?P<kw>\b(?:if|else|for|while|do|switch|case|break|continue|return|class|struct|namespace|public|private|protected|template|typename|using|auto|const|constexpr|static|inline|virtual|override|final|nullptr|true|false|new|delete|try|catch|throw|sizeof|enum|volatile|mutable|friend|operator|explicit)\b)"
)


def highlight_cpp_tokens(code: str) -> str:
    result: list[str] = []
    last = 0
    for match in CPP_TOKEN.finditer(code):
        if match.start() > last:
            result.append(html.escape(code[last:match.start()]))
        token = match.group(0)
        if match.lastgroup == "num":
            result.append(f"<span class=\"num\">{html.escape(token)}</span>")
        elif match.lastgroup == "type":
            result.append(f"<span class=\"type\">{html.escape(token)}</span>")
        else:
            result.append(f"<span class=\"kw\">{html.escape(token)}</span>")
        last = match.end()
    if last < len(code):
        result.append(html.escape(code[last:]))
    return "".join(result)


def escape_and_highlight(code: str) -> str:
    return highlight_cpp_tokens(code)


def split_line_comment(line: str) -> tuple[str, str]:
    in_string = False
    string_char = ""
    escape = False
    for i in range(len(line) - 1):
        c = line[i]
        n = line[i + 1]
        if escape:
            escape = False
            continue
        if in_string:
            if c == "\\":
                escape = True
            elif c == string_char:
                in_string = False
                string_char = ""
            continue
        if c in ('"', "'"):
            in_string = True
            string_char = c
            continue
        if c == "/" and n == "/":
            return line[:i], line[i:]
    return line, ""


def highlight_cpp_line(line: str) -> str:
    code, comment = split_line_comment(line)
    result_parts: list[str] = []

    def highlight_segment(segment: str) -> None:
        last = 0
        for match in CPP_STRING.finditer(segment):
            if match.start() > last:
                result_parts.append(escape_and_highlight(segment[last:match.start()]))
            result_parts.append(f"<span class=\"str\">{html.escape(match.group(0))}</span>")
            last = match.end()
        if last < len(segment):
            result_parts.append(escape_and_highlight(segment[last:]))

    highlight_segment(code)
    if comment:
        result_parts.append(f"<span class=\"com\">{html.escape(comment)}</span>")

    return "".join(result_parts)


def normalize_reference_path(text: str) -> str:
    path = text.replace("\\", "/")
    if path.startswith("Reference/"):
        path = path[len("Reference/") :]
    if path.startswith("./"):
        path = path[2:]
    path = os.path.normpath(path).replace("\\", "/")
    return "" if path == "." else path


def rel_link(from_rel: str, target_rel: str) -> str:
    from_dir = Path(from_rel).parent
    target_html = Path(target_rel).with_suffix(".html")
    link = Path(os.path.relpath(target_html.as_posix(), start=from_dir.as_posix() or "."))
    return link.as_posix()


def rel_asset(from_rel: str, asset_rel: str) -> str:
    from_dir = Path(from_rel).parent
    link = Path(os.path.relpath(asset_rel, start=from_dir.as_posix() or "."))
    return link.as_posix()


def rel_index(from_rel: str) -> str:
    if Path(from_rel).parts and Path(from_rel).parts[0] == "Tutorial":
        return rel_link(from_rel, "Tutorial/00_Index.md")
    name = Path(from_rel).stem
    # Utilities category pages -> Utilities Index
    if name.startswith("09_Utilities_") and name != "09_Utilities_Index":
        return rel_link(from_rel, "09_Utilities_Index.md")
    # Assets category pages -> Assets Index
    if name.startswith("11_Assets_") and name != "11_Assets_Index":
        return rel_link(from_rel, "11_Assets_Index.md")
    return rel_link(from_rel, "00_Index.md")


def resolve_reference(text: str, current_rel: str, reference_files: set[str]) -> tuple[str, str] | None:
    normalized = normalize_reference_path(text)
    path, _, fragment = normalized.partition("#")
    candidates = [path]
    if path and current_rel:
        combined = os.path.normpath(str(Path(current_rel).parent / path)).replace("\\", "/")
        candidates.append(combined)
    for candidate in candidates:
        if candidate in reference_files:
            return candidate, fragment
    return None


def convert_inline(text: str, current_rel: str, reference_files: set[str]) -> str:
    out = []
    i = 0
    while i < len(text):
        if text[i] == "`":
            j = text.find("`", i + 1)
            if j == -1:
                out.append(html.escape(text[i:]))
                break
            code = text[i + 1 : j]
            ref = resolve_reference(code, current_rel, reference_files)
            code_html = html.escape(code)
            if ref:
                href = rel_link(current_rel, ref[0])
                if ref[1]:
                    href = f"{href}#{html.escape(ref[1])}"
                out.append(f'<a href="{href}"><code>{code_html}</code></a>')
            else:
                out.append(f"<code>{code_html}</code>")
            i = j + 1
            continue
        if text[i] == "[":
            j = text.find("]", i + 1)
            if j != -1 and j + 1 < len(text) and text[j + 1] == "(":
                k = text.find(")", j + 2)
                if k != -1:
                    label = text[i + 1 : j]
                    url = text[j + 2 : k]
                    ref = resolve_reference(url, current_rel, reference_files)
                    if ref:
                        url = rel_link(current_rel, ref[0])
                        if ref[1]:
                            url = f"{url}#{html.escape(ref[1])}"
                    out.append(f'<a href="{html.escape(url)}">{html.escape(label)}</a>')
                    i = k + 1
                    continue
        out.append(html.escape(text[i]))
        i += 1
    return "".join(out)


def convert_markdown(md_text: str, current_rel: str, reference_files: set[str]) -> tuple[str, str]:
    md_text = md_text.lstrip("\ufeff")
    lines = md_text.splitlines()
    html_lines: list[str] = []
    in_code = False
    code_lang = ""
    list_levels: list[int] = []
    paragraph: list[str] = []
    in_blockquote = False
    title = ""

    def flush_paragraph() -> None:
        nonlocal paragraph
        if paragraph:
            html_lines.append(f"<p>{convert_inline(' '.join(paragraph), current_rel, reference_files)}</p>")
            paragraph = []

    def close_lists(target_level: int = 0) -> None:
        nonlocal list_levels
        while len(list_levels) > target_level:
            html_lines.append("</ul>")
            list_levels.pop()

    def close_blockquote() -> None:
        nonlocal in_blockquote
        if in_blockquote:
            html_lines.append("</blockquote>")
            in_blockquote = False

    for raw_line in lines:
        line = raw_line.rstrip("\n")
        if line.strip().startswith("```"):
            flush_paragraph()
            close_blockquote()
            if not in_code:
                code_lang = line.strip()[3:].strip()
                if not code_lang:
                    code_lang = "cpp"
                lang_class = f" class=\"language-{html.escape(code_lang)}\"" if code_lang else ""
                html_lines.append(f"<pre><code{lang_class}>")
                in_code = True
            else:
                html_lines.append("</code></pre>")
                in_code = False
            continue
        if in_code:
            if code_lang.lower() in {"cpp", "c", "c++", "cc", "hpp", "h"}:
                html_lines.append(highlight_cpp_line(line))
            else:
                html_lines.append(html.escape(line))
            continue

        if line.strip() == "":
            flush_paragraph()
            close_lists()
            close_blockquote()
            continue

        if line.strip() == "---":
            flush_paragraph()
            close_lists()
            close_blockquote()
            html_lines.append("<hr />")
            continue

        if line.lstrip().startswith("- "):
            flush_paragraph()
            close_blockquote()
            indent = len(line) - len(line.lstrip(" "))
            level = max(indent // 2, 0)
            while len(list_levels) < level + 1:
                html_lines.append("<ul>")
                list_levels.append(level)
            while len(list_levels) > level + 1:
                html_lines.append("</ul>")
                list_levels.pop()
            content = line.lstrip()[2:]
            html_lines.append(f"<li>{convert_inline(content, current_rel, reference_files)}</li>")
            continue

        if line.startswith("#"):
            flush_paragraph()
            close_lists()
            close_blockquote()
            level = len(line) - len(line.lstrip("#"))
            level = max(1, min(level, 6))
            content = line[level:].strip()
            converted = convert_inline(content, current_rel, reference_files)
            html_lines.append(f"<h{level}>{converted}</h{level}>")
            if not title and level == 1:
                title = html.escape(content)
            continue

        if line.lstrip().startswith("> "):
            flush_paragraph()
            close_lists()
            if not in_blockquote:
                html_lines.append("<blockquote>")
                in_blockquote = True
            content = line.lstrip()[2:]
            html_lines.append(f"<p>{convert_inline(content, current_rel, reference_files)}</p>")
            continue

        close_lists()
        close_blockquote()
        paragraph.append(line.strip())

    flush_paragraph()
    close_lists()
    close_blockquote()

    if not title:
        title = "Reference"

    body = "\n".join(html_lines)
    return title, body


def build_html(title: str, body: str, stylesheet_href: str, index_href: str) -> str:
    return "\n".join(
        [
            "<!DOCTYPE html>",
            "<html lang=\"ja\">",
            "<head>",
            "  <meta charset=\"utf-8\" />",
            f"  <title>{title}</title>",
            f"  <link rel=\"stylesheet\" href=\"{html.escape(stylesheet_href)}\" />",
            "</head>",
            "<body>",
            f"<nav><a href=\"{html.escape(index_href)}\">目次へ</a></nav>",
            body,
            "</body>",
            "</html>",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert Reference markdown files to HTML.")
    parser.add_argument("--root", default="Reference", help="Root directory containing Reference markdown.")
    args = parser.parse_args()

    root = Path(args.root)
    if not root.exists():
        raise SystemExit(f"Reference root not found: {root}")

    md_files = sorted(root.rglob("*.md"))
    reference_files = {p.relative_to(root).as_posix() for p in md_files}

    for md_file in md_files:
        rel = md_file.relative_to(root).as_posix()
        text = md_file.read_text(encoding="utf-8")
        title, body = convert_markdown(text, rel, reference_files)
        stylesheet_href = rel_asset(rel, "reference.css")
        index_href = rel_index(rel)
        html_text = build_html(title, body, stylesheet_href, index_href)
        output_path = md_file.with_suffix(".html")
        output_path.write_text(html_text, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
