"""Generate a candidate dictionary with Claude Opus 5.5; never edits production defaults.

Run from the repository root. Validate the candidate with RunShortTextDefaultsRegression.ps1
before copying it to Project/EditorTools/ShortTextDefaults.json.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT.parent / "Generated"

def object_schema(properties):
    return {"type": "object", "properties": properties, "required": list(properties), "additionalProperties": False}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--claude", default="claude")
    args = parser.parse_args()
    source = (ROOT / "KashipanEngine/Utilities/ShortTextProgram.h").read_text(encoding="utf-8-sig")
    def names(name):
        body = re.search(name + r"\s*=\s*\{([^}]+)\}", source).group(1)
        return re.findall(r'"([^"]+)"', body)
    events, tests, operations = names("EventNames"), names("TestNames"), names("OperationNames")
    keys = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") + ["Left", "Right", "Up", "Down", "Space", "Enter", "Shift", "Control", "Escape", "Tab"]
    axis = {"type": "integer", "minimum": 0, "maximum": 2}
    number = {"type": "number", "minimum": -10000, "maximum": 10000}
    condition = object_schema({"test": {"type": "string", "enum": tests}, "axis": axis, "value": number})
    action = object_schema({"operation": {"type": "string", "enum": operations}, "axis": axis, "value": number})
    rule = object_schema({"event": {"type": "string", "enum": events}, "key": {"type": "string", "enum": keys},
                          "interval": {"type": "number", "minimum": 0.05, "maximum": 3600},
                          "conditions": {"type": "array", "items": condition, "maxItems": 4},
                          "actions": {"type": "array", "items": action, "minItems": 1, "maxItems": 8}})
    recipe = object_schema({"kind": {"type": "integer", "const": 3},
                            "rules": {"type": "array", "items": rule, "minItems": 1, "maxItems": 16}})
    entry = object_schema({"mode": {"type": "string", "const": "recipe"},
                           "phrase": {"type": "string", "maxLength": 170, "minLength": 1}, "recipe": recipe})
    schema = object_schema({"version": {"type": "integer", "const": 1},
                            "entries": {"type": "array", "items": entry, "minItems": 54, "maxItems": 90}})
    prompt = """あなたはKashipanEngineの標準例文辞書を生成します。渡したJSON Schemaに厳密に従い、日本語で18種類以上の異なる挙動と、それぞれ3種類の自然な短文を作成してください。各例文に完成したrecipeを入れ、同じ挙動の言い換えには完全に同じrecipeを複製してください。phraseは正規化後も重複不可です。説明文やコードは出力しません。
実装済み仕様:
- kind=3はイベントと条件と動作のプログラムです。ルールは配列順に実行し、動作も配列順です。条件はAND、空配列は無条件です。対象は選択した一つのオブジェクト自身だけです。
- 軸はX=0,Y=1,Z=2。座標・速度は親を基準にしたローカル座標です。右はX正、上はY正、前はZ正。回転値は度です。
- startは再生開始後の最初の更新で一度。every_frameは毎更新。key_pressed/held/releasedは指定キーの押した瞬間/長押し/離した瞬間。timerはinterval秒おき。1フレームで最大1回で、余った時間を保持します。dtは0〜1秒です。
- set_velocity/add_velocity/set_acceleration/stop_velocityはVelocityの指定軸の速度・加速度を操作します。Velocityが必要なレシピには適用時にVelocityを追加します。Velocityはこのプログラムの後に加速度→速度→位置を更新します。初期速度・加速度は0です。
- translateは操作値×dtだけ位置を加算（単位/秒）、rotateは操作値×dtだけ回転を加算（度/秒）。timerでrotateを指定しても瞬間的に操作値度回転するわけではありません。
- set_position/set_rotation/set_scaleはその軸へ指定値を代入。set_scaleは0より大きく100以下です。add_velocityはイベントごとに値を加算し、dtを掛けません。stop_velocityは指定軸を0にしvalue=0とします。
- position_at_most/at_leastは指定軸の位置<=/>=value。velocity_at_most/at_leastは速度<=/>=valueです。衝突・接地・物理コントローラの機能はありません。床Y=0の例は位置と速度による簡易床として明示してください。
- 非キーイベントのkeyはSpace、非タイマーイベントのintervalは1を入れてください。
- 全6イベント、全4条件、全9動作を辞書全体で最低一回ずつ使ってください。
含める挙動:
1 スペース押下でY速度16（重力や接地を付けない）。この挙動のphraseに必ず「スペースキーを押したらvelocityに16.0fを代入する」を含める。
2 開始時にY加速度-30で落下。
3 位置Y<=0かつ速度Y<=0でY位置/速度/加速度を0へ戻す簡易床。
4 簡易床Y=0からスペースでジャンプ。開始時加速度-30、床条件で位置/速度/加速度0、スペース押下かつ位置Y<=0かつ速度Y<=0で速度16と加速度-30。床処理をジャンプより前に置く。
5 D長押しで毎秒4の右移動、6 A長押しで毎秒4の左移動。
7 右矢印長押しでX速度4、離すとX速度0。
8 Shift長押しでX速度8、離すとX速度4。
9 R押下でXYZ位置とXYZ速度を0へリセット。
10 E押下でXYZスケール2、11 Q押下でXYZスケール1。
12 開始時にY回転90度。
13 1秒ごとにX速度を1加算。
14 開始時にXYZスケール2、その後毎フレームZ回転45度/秒。
15 開始時X速度2、X位置>=5でX速度-2、X位置<=-5でX速度2にする往復。
16 Y速度<=-0.001でYスケール0.8、Y速度>=0.001でYスケール1.2。
17 Enterを離した瞬間にY位置2。
18 2秒ごとにXYZスケール1.5を設定。
set_scaleは元のスケールを倍にする操作ではなく絶対値の代入です。「2倍」「元の大きさ」といった相対表現を避け、「スケールを2に設定」のように書いてください。位置5で速度を反転しても厳密に5以下へ位置が制限されるわけではないので、往復は境界「付近」と表現してください。
意味が実装と異なる言い換え、未実装の操作、対象参照、任意コードは使わないでください。
"""
    OUTPUT.mkdir(exist_ok=True)
    (OUTPUT / "ShortTextGeneration.schema.json").write_text(json.dumps(schema, ensure_ascii=False, indent=2), encoding="utf-8")
    (OUTPUT / "ShortTextGeneration.prompt.txt").write_text(prompt, encoding="utf-8")
    empty_mcp = OUTPUT / "ShortTextGeneration.mcp.json"
    empty_mcp.write_text('{"mcpServers":{}}', encoding="utf-8")
    command = [args.claude, "-p", "--model", "claude-opus-5-5", "--output-format", "json",
               "--json-schema", json.dumps(schema, ensure_ascii=False), "--tools", "", "--setting-sources", "",
               "--strict-mcp-config", "--mcp-config", str(empty_mcp), "--no-session-persistence", "--max-budget-usd", "5"]
    result = subprocess.run(command, input=prompt, capture_output=True, text=True, encoding="utf-8", timeout=300, cwd=ROOT.parent)
    (OUTPUT / "ShortTextGeneration.raw.json").write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError("Claude generation failed: " + result.stderr[-2000:] + result.stdout[-1000:])
    response = json.loads(result.stdout)
    if response.get("is_error"):
        raise RuntimeError("Claude returned an error: " + str(response.get("result", "")))
    document = response.get("structured_output")
    if not isinstance(document, dict):
        raise RuntimeError("Claude did not return a structured candidate")
    candidate = OUTPUT / "ShortTextDefaults.candidate.json"
    candidate.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report = {"requestedModel": "claude-opus-5-5", "models": list(response.get("modelUsage", {})),
              "phrases": len(document.get("entries", [])), "durationMs": response.get("duration_ms"),
              "costUsd": response.get("total_cost_usd")}
    (OUTPUT / "ShortTextGenerationReport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    print("Candidate: " + str(candidate))

if __name__ == "__main__":
    main()
