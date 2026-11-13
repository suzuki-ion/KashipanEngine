"""Run a real cooked distribution after relocation; preserves QA data under Generated."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import uuid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("distribution", type=Path)
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    qa = repo / "Generated" / ("CookedRuntimeRegression-" + uuid.uuid4().hex)
    package = qa / "日本語 配布"
    shutil.copytree(args.distribution, package, ignore=shutil.ignore_patterns("Cache", "Logs", "SaveData"))
    manifest = json.loads((package / "Cooked/Manifest.json").read_text(encoding="utf-8"))
    assert not list(package.rglob("*.prefab")), "Release must not contain prefab files or directories"
    assert not any(any(part.lower().endswith(".prefab") for part in Path(name).parts) for name in manifest["assets"])
    counts = {}
    for key in manifest["entries"]:
        category, logical, _ = key.split("|", 2)
        counts[category] = counts.get(category, 0) + 1
        if category in ("Textures", "Models"):
            assert not (package / "Assets" / logical).exists(), logical
        data = (package / "Cooked" / (hashlib.sha256(key.encode()).hexdigest() + ".cache")).read_bytes()
        length = struct.unpack_from("<Q", data)[0]
        metadata = json.loads(data[8:8 + length])
        assert metadata["key"] == key and metadata["dependencies"] == []
        assert hashlib.sha256(data[8 + length:]).hexdigest() == metadata["hash"]
    assert not list((package / "Assets").rglob("*.hlsl"))
    assert not list((package / "Assets").rglob("*.hlsli"))
    scene_file = package / "Assets/SceneList.json"
    scene_list = json.loads(scene_file.read_text(encoding="utf-8-sig"))
    scenes = [entry["name"] for entry in scene_list["scenes"]]
    # Repeating the first scene also checks reuse of locally generated PSOs.
    if scenes:
        scenes.append(scenes[0])
    results = []
    for number, scene in enumerate(scenes):
        scene_list["startupScene"] = scene
        scene_file.write_text(json.dumps(scene_list, ensure_ascii=False, indent=2), encoding="utf-8")
        previous_logs = set((package / "Logs").rglob("*.log")) if (package / "Logs").exists() else set()
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        process = subprocess.Popen([str(package / "KashipanEngine.exe")], cwd=qa, startupinfo=startup)
        text = ""
        try:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                logs = set((package / "Logs").rglob("*.log")) - previous_logs
                text = "\n".join(path.read_text(encoding="utf-8-sig", errors="replace") for path in sorted(logs))
                if "Startup.Total (" in text:
                    time.sleep(2)
                    text = "\n".join(path.read_text(encoding="utf-8-sig", errors="replace") for path in sorted(logs))
                    break
                if process.poll() is not None:
                    raise RuntimeError(f"Engine exited before first frame: {scene}, {process.returncode}")
                time.sleep(0.25)
            else:
                raise RuntimeError(f"Startup timed out: {scene}")
            errors = [line for line in text.splitlines() if "[ERROR]" in line or "[CRITICAL]" in line]
            (qa / f"{number}-{scene}.log").write_text(text, encoding="utf-8")
            assert not errors, "\n".join(errors[:12])
            timing = re.search(r"Startup.Total .*?: ([\d.]+) ms", text)
            results.append({"scene": scene, "startupMs": float(timing[1]) if timing else None,
                            "cacheStats": [line.strip() for line in text.splitlines() if "[StartupCache]" in line]})
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=15)
    report = {"package": str(package), "entries": counts, "runs": results,
              "scope": "First scene draw and two seconds of updates; no visual verification."}
    (qa / "Results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
