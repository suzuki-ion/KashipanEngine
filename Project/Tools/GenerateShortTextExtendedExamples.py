"""Generate extended short-text phrases with Claude Opus 5.5; never edits production defaults.

Seed recipes are approved, native-validated recipes. Claude only proposes Japanese phrases for
each seed id; the seed recipe is copied verbatim into the candidate dictionary.

Run from the repository root, e.g.
    python Project/Tools/GenerateShortTextExtendedExamples.py --seeds Project/Docs/ShortTextExtendedSeeds.json

Validate the candidate with RunShortTextDefaultsRegression.ps1 before copying it to
Project/EditorTools/ShortTextDefaults.json.
"""
from pathlib import Path
from collections import Counter
import argparse
import copy
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT.parent / "Generated"
RAW_DIR = OUTPUT / "ShortTextExtended"
PRODUCTION = ROOT / "EditorTools" / "ShortTextDefaults.json"
CANDIDATE = OUTPUT / "ShortTextExtendedDefaults.candidate.json"
REPORT = OUTPUT / "ShortTextExtendedDefaults.report.json"

MODEL = "claude-opus-5-5"
PHRASES_PER_SEED = 3
MAX_PHRASE_BYTES = 512
MAX_BUDGET_USD = 5.0
TIMEOUT_SECONDS = 300

sys.path.insert(0, str(Path(__file__).resolve().parent))
from GenerateShortTextExamples import merge_entries  # noqa: E402


def object_schema(properties):
    return {"type": "object", "properties": properties, "required": list(properties), "additionalProperties": False}


def response_schema():
    phrase = {"type": "string", "minLength": 1}
    item = object_schema({
        "id": {"type": "string"},
        "phrases": {"type": "array", "items": phrase, "minItems": PHRASES_PER_SEED, "maxItems": PHRASES_PER_SEED},
    })
    return object_schema({"items": {"type": "array", "items": item}})


def load_json(path):
    with open(path, "r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with open(temporary, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    os.replace(temporary, path)


def load_seeds(path):
    document = load_json(path)
    recipes = document.get("recipes") if isinstance(document, dict) else None
    if not isinstance(recipes, list) or not recipes:
        raise RuntimeError("Seed file must contain a nonempty 'recipes' list: " + str(path))
    seen = set()
    for index, seed in enumerate(recipes):
        if not isinstance(seed, dict):
            raise RuntimeError("Seed #%d is not an object" % index)
        for key in ("id", "description", "requirements", "recipe"):
            if key not in seed:
                raise RuntimeError("Seed #%d is missing '%s'" % (index, key))
        if not isinstance(seed["id"], str) or not seed["id"].strip():
            raise RuntimeError("Seed #%d has an invalid id" % index)
        if seed["id"] in seen:
            raise RuntimeError("Duplicate seed id: " + seed["id"])
        if not isinstance(seed["recipe"], dict):
            raise RuntimeError("Seed recipe must be an object: " + seed["id"])
        seen.add(seed["id"])
    return recipes


def find_entries_key(document):
    if not isinstance(document, dict):
        raise RuntimeError("Production defaults must be a JSON object")
    keys = [key for key, value in document.items()
            if isinstance(value, list) and value
            and all(isinstance(item, dict) and {"phrase", "mode", "recipe"} <= item.keys() for item in value)]
    if len(keys) != 1:
        raise RuntimeError("Could not identify the phrase entry list in production defaults (found %r)" % keys)
    return keys[0]


def build_prompt(chunk):
    seeds = [{"id": seed["id"], "description": seed["description"], "requirements": seed["requirements"],
              "recipe": seed["recipe"]} for seed in chunk]
    return "\n".join([
        "You write short Japanese phrases for a game engine editor dictionary.",
        "Each seed below is an approved, natively validated behavior recipe. The recipes are read-only context:",
        "do NOT modify, restate, or return any recipe, JSON, or code.",
        "",
        "For every seed id, return exactly %d distinct, short, natural Japanese phrases that a user would type"
        " to request exactly that behavior." % PHRASES_PER_SEED,
        "Rules:",
        "- Object names, tags, variable names, keys/buttons, and numbers (speeds, durations, counts, axes, colors)"
        " must match the recipe exactly. Do not invent, round, or omit values that define the behavior.",
        "- Respect the stated requirements; do not describe behavior the recipe does not perform.",
        "- Do not claim the system understands arbitrary user text, and do not say physics or any feature is missing"
        " or unsupported.",
        "- Each phrase is a single line, at most 150 Japanese characters and 512 UTF-8 bytes. Prefer concise phrasing but keep essential values and names. No recipe JSON or code.",
        "- Return every seed id exactly once and no other ids.",
        "",
        "Seeds:",
        json.dumps(seeds, ensure_ascii=False, indent=2),
    ])


def resolve_claude(command):
    resolved = shutil.which(command)
    if not resolved:
        raise RuntimeError("Claude CLI not found: " + command)
    return resolved


def run_claude(claude, prompt):
    command = [
        claude, "-p",
        "--model", MODEL,
        "--tools", "",
        "--setting-sources", "",
        "--strict-mcp-config", "--mcp-config", json.dumps({"mcpServers": {}}),
        "--no-session-persistence",
        "--max-budget-usd", str(int(MAX_BUDGET_USD)),
        "--output-format", "json",
        "--json-schema", json.dumps(response_schema()),
    ]
    started = time.monotonic()
    completed = subprocess.run(command, input=prompt, capture_output=True, text=True, encoding="utf-8",
                               timeout=TIMEOUT_SECONDS, cwd=str(ROOT.parent))
    elapsed = time.monotonic() - started
    return completed, elapsed


def parse_output(stdout):
    output = json.loads(stdout)
    if not isinstance(output, dict):
        raise RuntimeError("Claude CLI output is not a JSON object")
    if output.get("is_error") or output.get("subtype") not in (None, "success"):
        raise RuntimeError("Claude CLI reported an error: %s" % output.get("subtype"))
    structured = output.get("structured_output")
    if structured is None:
        result = output.get("result")
        if not isinstance(result, str):
            raise RuntimeError("Claude CLI output has no structured result")
        structured = json.loads(result)
    return output, structured


def verify_model(output):
    usage = output.get("modelUsage")
    if not isinstance(usage, dict) or not usage:
        raise RuntimeError("Claude CLI output has no modelUsage")
    models = sorted(usage)
    if not any(name == MODEL or name.startswith(MODEL + "[") or name.startswith(MODEL + "-") for name in models):
        raise RuntimeError("modelUsage does not include %s: %r" % (MODEL, models))
    return models


def validate_phrases(chunk, structured):
    if not isinstance(structured, dict) or set(structured) != {"items"} or not isinstance(structured["items"], list):
        raise RuntimeError("Claude response must be exactly {items:[...]}")
    expected = [seed["id"] for seed in chunk]
    result = {}
    for item in structured["items"]:
        if not isinstance(item, dict) or set(item) != {"id", "phrases"}:
            raise RuntimeError("Claude response item must contain only id and phrases: %r" % (item,))
        seed_id = item["id"]
        if seed_id not in expected:
            raise RuntimeError("Claude returned an unknown seed id: %r" % (seed_id,))
        if seed_id in result:
            raise RuntimeError("Claude returned seed id more than once: " + seed_id)
        phrases = item["phrases"]
        if not isinstance(phrases, list) or len(phrases) != PHRASES_PER_SEED:
            raise RuntimeError("Seed %s must have exactly %d phrases" % (seed_id, PHRASES_PER_SEED))
        cleaned = []
        for phrase in phrases:
            if not isinstance(phrase, str):
                raise RuntimeError("Seed %s has a non-string phrase" % seed_id)
            phrase = phrase.strip()
            if not phrase:
                raise RuntimeError("Seed %s has an empty phrase" % seed_id)
            if "\n" in phrase or "\r" in phrase:
                raise RuntimeError("Seed %s has a multi-line phrase" % seed_id)
            if any(token in phrase for token in ("{", "}", "`", '"rules"')):
                raise RuntimeError("Seed %s phrase looks like recipe or code: %s" % (seed_id, phrase))
            try:
                size = len(phrase.encode("utf-8"))
            except UnicodeEncodeError as error:
                raise RuntimeError("Seed %s phrase is not valid UTF-8: %s" % (seed_id, error))
            if size > MAX_PHRASE_BYTES:
                raise RuntimeError("Seed %s phrase exceeds %d UTF-8 bytes" % (seed_id, MAX_PHRASE_BYTES))
            cleaned.append(phrase)
        result[seed_id] = cleaned
    missing = [seed_id for seed_id in expected if seed_id not in result]
    if missing:
        raise RuntimeError("Claude omitted seed ids: %r" % missing)
    return result


def default_mode(entries, override):
    if override:
        return override
    counts = Counter(entry["mode"] for entry in entries)
    if not counts:
        raise RuntimeError("Cannot infer entry mode; pass --mode")
    return counts.most_common(1)[0][0]


def chunk_range(value):
    number = int(value)
    if not 1 <= number <= 20:
        raise argparse.ArgumentTypeError("--chunk-size must be between 1 and 20")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--claude", default="claude")
    parser.add_argument("--seeds", required=True, type=Path)
    parser.add_argument("--base-dictionary", type=Path, default=PRODUCTION,
                        help="validated previous dictionary or candidate to extend")
    parser.add_argument("--chunk-size", type=chunk_range, default=12)
    parser.add_argument("--mode", default="", help="entry mode for seeds without 'mode' (default: most common existing mode)")
    args = parser.parse_args()

    report = {"model": MODEL, "seeds": str(args.seeds), "chunkSize": args.chunk_size, "chunks": [],
              "models": [], "totalCostUsd": 0.0, "totalDurationMs": 0, "seedCount": 0, "phraseCount": 0,
              "candidate": None, "status": "failed", "error": None}
    started = time.monotonic()
    try:
        seeds = load_seeds(args.seeds)
        original_seeds = copy.deepcopy(seeds)
        report["seedCount"] = len(seeds)

        previous_document = load_json(args.base_dictionary)
        entries_key = find_entries_key(previous_document)
        previous_entries = previous_document[entries_key]
        fallback_mode = default_mode(previous_entries, args.mode)

        claude = resolve_claude(args.claude)
        RAW_DIR.mkdir(parents=True, exist_ok=True)
        phrases_by_id = {}
        models = set()
        chunks = [seeds[i:i + args.chunk_size] for i in range(0, len(seeds), args.chunk_size)]
        for number, chunk in enumerate(chunks, 1):
            print("Generating chunk %d/%d (%d recipes)" % (number, len(chunks), len(chunk)), flush=True)
            chunk_report = {"index": number, "seedIds": [seed["id"] for seed in chunk]}
            report["chunks"].append(chunk_report)
            completed, elapsed = run_claude(claude, build_prompt(chunk))
            raw_path = RAW_DIR / ("chunk-%03d.raw.json" % number)
            write_json(raw_path, {"returncode": completed.returncode, "stdout": completed.stdout,
                                  "stderr": completed.stderr, "elapsedSeconds": round(elapsed, 3)})
            chunk_report["raw"] = str(raw_path)
            if completed.returncode != 0:
                raise RuntimeError("Claude CLI failed for chunk %d (exit %d): %s"
                                   % (number, completed.returncode, completed.stderr.strip()[:500]))
            output, structured = parse_output(completed.stdout)
            chunk_models = verify_model(output)
            cost = float(output.get("total_cost_usd") or 0.0)
            duration = int(output.get("duration_ms") or round(elapsed * 1000))
            chunk_report.update({"models": chunk_models, "costUsd": cost, "durationMs": duration})
            models.update(chunk_models)
            report["totalCostUsd"] += cost
            report["totalDurationMs"] += duration
            if report["totalCostUsd"] > MAX_BUDGET_USD:
                raise RuntimeError("Total cost exceeded $%.2f" % MAX_BUDGET_USD)
            chunk_phrases = validate_phrases(chunk, structured)
            chunk_report["phraseCount"] = sum(len(p) for p in chunk_phrases.values())
            phrases_by_id.update(chunk_phrases)
        report["models"] = sorted(models)

        if seeds != original_seeds:
            raise RuntimeError("Seed recipes were modified during generation")

        generated = []
        for seed in original_seeds:
            for phrase in phrases_by_id[seed["id"]]:
                generated.append({"phrase": phrase, "mode": seed.get("mode", fallback_mode),
                                  "recipe": copy.deepcopy(seed["recipe"])})
        report["phraseCount"] = len(generated)

        merged = merge_entries(previous_entries, generated)
        for entry in generated:
            if entry["recipe"] != next(s["recipe"] for s in original_seeds
                                       if entry["phrase"] in phrases_by_id[s["id"]]):
                raise RuntimeError("Candidate recipe differs from seed for phrase: " + entry["phrase"])

        candidate = dict(previous_document)
        candidate[entries_key] = merged
        report["addedEntries"] = len(merged) - len(previous_entries)
        report["previousEntries"] = len(previous_entries)
        write_json(CANDIDATE, candidate)
        report["candidate"] = str(CANDIDATE)
        report["status"] = "succeeded"
    except Exception as error:  # report every failure; candidate is never promoted on error
        report["error"] = "%s: %s" % (type(error).__name__, error)
    finally:
        report["wallClockSeconds"] = round(time.monotonic() - started, 3)
        write_json(REPORT, report)

    if report["status"] != "succeeded":
        print("Generation failed; no candidate written: " + report["error"], file=sys.stderr)
        print("Report: " + str(REPORT), file=sys.stderr)
        return 1
    print("Wrote %s (%d seeds, %d phrases, %d new entries, $%.4f, models %s)"
          % (CANDIDATE, report["seedCount"], report["phraseCount"], report["addedEntries"],
             report["totalCostUsd"], ", ".join(report["models"])))
    print("Production defaults were not modified. Validate before copying to " + str(PRODUCTION))
    return 0


if __name__ == "__main__":
    sys.exit(main())
