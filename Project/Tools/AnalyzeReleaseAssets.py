"""Conservative static asset scan for the Release distribution copy.

All scene, script, and other text assets are scanned. Files with unknown or
dynamic dependencies remain in the package. Only unreferenced media files are
eligible for automatic removal; the source Assets tree is never modified.
"""

import argparse
import json
import os
import re
from pathlib import Path


TEXT_EXTENSIONS = {
    ".as", ".json", ".jsonc", ".mat", ".prefab", ".scene", ".hlsl",
    ".hlsli", ".gltf", ".obj", ".mtl", ".fnt", ".txt", ".xml", ".ini",
    ".html", ".css", ".js", ".csv", ".yml", ".yaml",
}
MEDIA_TYPES = {
    ".png": "image", ".jpg": "image", ".jpeg": "image", ".bmp": "image",
    ".dds": "image", ".tga": "image", ".webp": "image", ".gif": "image",
    ".ico": "image", ".mp3": "audio", ".wav": "audio", ".ogg": "audio",
    ".flac": "audio", ".mp4": "video", ".webm": "video", ".avi": "video",
    ".fbx": "model", ".glb": "model", ".bin": "model",
}
SCRIPT_CALLS = {
    "audio": re.compile(r"\b(?:PlayAudio|SetSoundName)\s*\(", re.I),
    "model": re.compile(r"\b(?:GetModelHandleFromAssetPath|SetAnimationSourceAssetPath)\s*\(", re.I),
    "image": re.compile(r"\b(?:Set\w*Texture\w*|SetGifAssetPath)\s*\(", re.I),
    "video": re.compile(r"\bSetVideoAssetPath\s*\(", re.I),
}
QUOTED = re.compile(r"[\"']([^\"'\r\n]{1,2048})[\"']")
FILE_TOKEN = re.compile(r"[\w./\\()\-]+\.[A-Za-z0-9]{1,8}")


def asset_files(root: Path) -> list[Path]:
    result = []
    for directory, dirs, files in os.walk(root, followlinks=False):
        for name in dirs:
            if (Path(directory) / name).is_symlink():
                raise RuntimeError(f"Cannot scan linked asset directory: {Path(directory) / name}")
        for name in files:
            path = Path(directory) / name
            if path.is_symlink():
                raise RuntimeError(f"Cannot scan linked asset file: {path}")
            result.append(path)
    return sorted(result)


def add_reference(value: str, references: set[str]) -> None:
    value = value.replace("\\/", "/").replace("\\", "/").strip().lower()
    if not value:
        return
    if value.startswith("./"):
        value = value[2:]
    if value.startswith("assets/"):
        value = value[7:]
    references.add(value)
    name = value.rsplit("/", 1)[-1]
    references.add(name)
    stem = name.rsplit(".", 1)[0]
    if len(stem) >= 4:
        references.add(stem)


def scan(root: Path) -> dict:
    files = asset_files(root)
    references: set[str] = set()
    dynamic_types: set[str] = set()
    text_count = 0

    # Scan every scene and AngelScript file, plus material/model/shader metadata
    # so that transitive references stay in the package.
    for path in files:
        if path.suffix.lower() not in TEXT_EXTENSIONS:
            continue
        content = path.read_text(encoding="utf-8-sig", errors="strict")
        text_count += 1
        for value in QUOTED.findall(content):
            add_reference(value, references)
        for value in FILE_TOKEN.findall(content):
            add_reference(value, references)
        if path.suffix.lower() == ".as":
            for kind, pattern in SCRIPT_CALLS.items():
                for match in pattern.finditer(content):
                    argument = content[match.end():].lstrip()
                    # Only a complete literal first argument has a known path.
                    literal = re.match(r'([\"\'])(?:\\.|(?!\1).)*?\1\s*(?=[,)])', argument, re.S)
                    if literal is None:
                        dynamic_types.add(kind)
                        break
            # A file extension assembled with string concatenation can refer to
            # any file of that media type, even if no complete name occurs.
            for suffix, kind in MEDIA_TYPES.items():
                if re.search(r"[\"']" + re.escape(suffix) + r"[\"']\s*\+|\+\s*[\"']" + re.escape(suffix) + r"[\"']", content, re.I):
                    dynamic_types.add(kind)

    # Binary models may contain external texture paths that a text scan cannot read.
    if any(path.suffix.lower() in {".fbx", ".glb"} for path in files):
        dynamic_types.add("image")

    excluded = []
    retained_unknown = []
    for path in files:
        relative = path.relative_to(root).as_posix()
        lower = relative.lower()
        if lower.startswith("kashipanengine/"):
            continue  # Engine-owned assets are outside the game-asset decision.
        kind = MEDIA_TYPES.get(path.suffix.lower())
        if kind is None:
            continue  # All text and unknown binary formats are retained.
        name = path.name.lower()
        stem = path.stem.lower()
        if lower in references or name in references or (len(stem) >= 4 and stem in references):
            continue
        if kind in dynamic_types:
            retained_unknown.append(relative)
            continue
        excluded.append(relative)

    return {
        "scannedFiles": len(files),
        "scannedTextFiles": text_count,
        "dynamicMediaTypes": sorted(dynamic_types),
        "excluded": excluded,
        "retainedUnknown": retained_unknown,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-root", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    if args.assets_root.is_symlink():
        parser.error("--assets-root must not be a linked directory")
    root = args.assets_root.resolve(strict=True)
    if not root.is_dir():
        parser.error("--assets-root must be a directory")
    result = scan(root)
    if args.apply:
        for relative in result["excluded"]:
            target = root / relative
            if not target.resolve().is_relative_to(root) or target.is_symlink():
                raise RuntimeError(f"Unsafe asset path: {relative}")
            target.unlink()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Asset scan: {result['scannedFiles']} files, "
          f"{len(result['excluded'])} excluded, "
          f"{len(result['retainedUnknown'])} retained as unknown")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
