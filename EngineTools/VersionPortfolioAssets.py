"""Version local CSS/JS URLs so a new HTML page cannot reuse stale assets."""
from pathlib import Path
import hashlib
import re
import sys
from urllib.parse import urlsplit


def version_assets(root):
    root = Path(root).resolve()
    if not (root / "index.html").is_file():
        raise ValueError("Expected the portfolio output directory")
    count = 0
    for page in root.rglob("*.html"):
        source = page.read_text(encoding="utf-8")

        def replace(match):
            url = urlsplit(match[2])
            if url.scheme or url.netloc or not url.path.endswith((".css", ".js")):
                return match[0]
            asset = (page.parent / url.path).resolve()
            if not asset.is_relative_to(root) or not asset.is_file():
                raise ValueError(f"Missing local asset: {page}: {url.path}")
            digest = hashlib.sha256(asset.read_bytes()).hexdigest()[:12]
            return match[1] + url.path + "?v=" + digest + match[3]

        updated = re.sub(r'(\b(?:href|src)=")([^"<>]+)(")', replace, source)
        if updated != source:
            page.write_text(updated, encoding="utf-8")
            count += 1
    return count


if __name__ == "__main__":
    print(f"Versioned assets in {version_assets(sys.argv[1])} portfolio pages")
