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

def phrase_key(text):
    translations = {ord(chr(0xFF10 + i)): str(i) for i in range(10)}
    translations.update({ord(c): c.lower() for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"})
    translations.update({ord(c): "" for c in " \t\r\n!　。、！"})
    translations.update({ord(a): b for a, b in zip("ＸＹＺｘｙｚ．－＋", "xyzxyz.-+")})
    return text.translate(translations)

def canonical_recipe(recipe):
    recipe = json.loads(json.dumps(recipe))
    for rule in recipe.get("rules", []):
        rule.setdefault("tag", "")
        rule.setdefault("button", 0); rule.setdefault("once", False); rule.setdefault("variable", "")
        for key,value in {"cooldown":0,"state":"","windowStart":0,"windowEnd":-1}.items(): rule.setdefault(key,value)
        for field in rule.get("conditions", []) + rule.get("actions", []):
            field.setdefault("name", "")
            field.setdefault("data", {})
        for action in rule.get("actions", []):
            for key,value in {"target":"self","targetName":"","reference":"","text":"","duration":1}.items(): action.setdefault(key,value)
    return recipe

def merge_entries(previous, generated):
    result = list(previous)
    known = {phrase_key(entry["phrase"]): entry for entry in previous}
    batch = set()
    for entry in generated:
        key = phrase_key(entry["phrase"])
        if key in batch:
            raise RuntimeError("Claude generated duplicate phrases: " + entry["phrase"])
        batch.add(key)
        old = known.get(key)
        if old:
            if old["mode"] != entry["mode"] or canonical_recipe(old["recipe"]) != canonical_recipe(entry["recipe"]):
                raise RuntimeError("Existing phrase has different behavior: " + entry["phrase"])
            continue
        known[key] = entry
        result.append(entry)
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--claude", default="claude")
    parser.add_argument("--batch", choices=["baseline", "interactions", "orchestration"], default="orchestration")
    args = parser.parse_args()
    source = (ROOT / "KashipanEngine/Utilities/ShortTextProgram.h").read_text(encoding="utf-8-sig")
    def names(name):
        body = re.search(name + r"\s*=\s*\{([^}]+)\}", source).group(1)
        return re.findall(r'"([^"]+)"', body)
    events, tests, operations = names("EventNames"), names("TestNames"), names("OperationNames")
    keys = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") + ["Left", "Right", "Up", "Down", "Space", "Enter", "Shift", "Control", "Escape", "Tab"]
    axis = {"type": "integer", "minimum": 0, "maximum": 2}
    number = {"type": "number", "minimum": -10000, "maximum": 10000}
    condition = object_schema({"test": {"type": "string", "enum": tests}, "axis": axis, "value": number, "name": {"type": "string", "maxLength": 64}})
    action = object_schema({"operation": {"type": "string", "enum": operations}, "axis": axis, "value": number, "name": {"type": "string", "maxLength": 64}})
    action["properties"].update({"target":{"type":"string","enum":["self","contact","name","tag"]},"targetName":{"type":"string","maxLength":64},
        "reference":{"type":"string","maxLength":64},"text":{"type":"string","maxLength":170},"duration":{"type":"number","minimum":0.01,"maximum":3600}})
    action["required"] = list(action["properties"])
    rule = object_schema({"event": {"type": "string", "enum": events}, "key": {"type": "string", "enum": keys},
                          "interval": {"type": "number", "minimum": 0.05, "maximum": 3600}, "tag": {"type": "string", "maxLength": 64},
                          "conditions": {"type": "array", "items": condition, "maxItems": 4},
                          "actions": {"type": "array", "items": action, "minItems": 1, "maxItems": 8}})
    rule["properties"].update({"button":{"type":"integer","minimum":0,"maximum":2},"once":{"type":"boolean"},"variable":{"type":"string","maxLength":64}})
    rule["required"] = list(rule["properties"])
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
    prompt += "\n全ルールにtag、全条件・全動作にnameを含める。用途がない場合は空文字を入れる。"
    if args.batch == "interactions":
        schema["properties"]["entries"]["minItems"] = 36
        schema["properties"]["entries"]["maxItems"] = 60
        prompt = """KashipanEngineの新規例文を生成する。JSON Schemaに厳密に従い、以下の12種類の挙動それぞれに3つの短い日本語例文を付ける。同じ挙動の3例文には完全に同じrecipeを複製する。既存の移動例文は生成しない。全entryはmode=recipe、kind=3。
仕様:
- 既存イベントはevery_frame/start/key_pressed/key_held/key_released/timer。追加イベントはcontact_enter/contact_stay/contact_exit。対象自身にColliderが必要で、rule.tagは接触相手オブジェクトのタグ。PlayerはPlayerタグのオブジェクト、空文字は全タグ。接触は2D/3Dの重なり検出で、物理接地や衝突応答ではない。
- 相手オブジェクトIDごとに開始/継続/終了を検出する。複数Colliderでも同じ相手は1件。最大32相手。contact_stayは毎フレーム、相手ごとに発火。条件は相手ごとに評価し、全条件を満たす場合だけ動作を配列順に実行する。
- 条件variable_at_most/variable_at_leastはnameのシーン数値変数がvalue以下/以上。未作成・文字列・boolなら条件は不成立。既存のposition_at_most/at_least,velocity_at_most/at_leastも使用可能。
- 動作set_variableはnameのシーン変数へvalueを代入。add_variableはvalueを加算、未作成は0から加算。float/double/intを扱い既存の数値型を保つ。数値でない既存変数は書き換えない。数値は±10億まで。
- play_sound/stop_sound/set_volumeは対象自身の最初のAudioSourceを操作する。音は既にユーザーがAudioSourceに設定済みという前提。playOnAwakeはfalseにする。Playは前の音を停止して同じ音源を再生し直す。複数音源や音声ファイルの生成・指定はできない。set_volumeのvalueは0〜1。Play/Stopのvalueは0。
- 既存のset_velocity/add_velocity/set_acceleration/translate/rotate/set_scale/set_position/set_rotation/stop_velocityも使える。軸X=0,Y=1,Z=2。translateは単位/秒、rotateは度/秒でdtを掛ける。その他はイベントごとの代入・加算。set_scaleは絶対値で0より大きく100以下。
- startは最初に一度、timerは0.05〜3600秒おきに最大1回/フレーム。ルール順に状態を更新する。キーSpaceなどは大文字小文字までSchemaに合わせる。
- 全ルールにtag,key,interval,conditions,actionsを入れる。非接触イベントのtagは空文字、非キーのkeyはSpace、非timerのintervalは1。
- 全条件・動作にaxis,value,nameを入れる。変数以外のnameは空文字、変数・音声操作のaxisは0。変数名はscore,hp,coinsを使う。任意コードや未実装の操作は不可。
必要な12挙動（各3例文）:
1 Playerに触れ始めたらscoreに1加算する。開始時にscoreをリセットしない。必ず「Playerに触れたらscoreを1増やす」というphraseを含める。
2 Playerに触れ始めたらscoreに1加算し、設定済みの音を再生する。必ず「Playerに触れたらscoreを1増やして音を鳴らす」を含める。
3 Playerに触れている間、相手1件につき毎フレームhpを1減らす。毎秒の減算とは言わない。
4 Playerから離れたら設定済みの音を再生する。
5 Playerに触れ始めたら音を再生し、離れたら音を停止する。
6 Spaceを押したら設定済みの音を再生する。
7 Escapeを押したら音を停止する。
8 開始時に音量0.25へ設定する。
9 開始時にscoreを0へ設定する。
10 1秒ごとにcoinsへ1加算する。
11 scoreが10以上のときPlayerに触れ始めたら、設定済みの音を再生する。
12 hpが0以下のときPlayerに触れ始めたら、hpを100に設定して設定済みの音を再生する。
言葉が動作の実態を超えないようにする。死亡/消滅/物理接地/接触相手の操作は未実装なので謳わない。音源とColliderの必要性は文に反映できる場合は明示する。JSON以外は出力しない。
"""
    if args.batch == "orchestration":
        schema["properties"]["entries"]["minItems"] = 66
        schema["properties"]["entries"]["maxItems"] = 90
        prompt = """KashipanEngineの追加辞書をJSON Schema通りに生成する。以下22種類の挙動それぞれに3つの短い自然な日本語例文を付ける。同じ挙動の言い換えは完全に同じrecipeを複製する。mode=recipe,kind=3。既存の移動・接触スコアの例文は生成しない。
仕様:
- ルールは順番に評価し、動作は順番に実行する。条件はAND。イベントは旧every_frame,start,key_pressed,key_held,key_released,timer,contact_enter,contact_stay,contact_exitに加えmouse_pressed,mouse_held,mouse_released,button_clicked,variable_changed,conditions_entered。
- buttonはマウス左0/右1/中2。button_clickedは所有オブジェクト自身のUIButtonの直近更新でのクリック結果。最初の更新では過去のクリックを消費しない。variable_changedはrule.variableの数値変化、初回は基準記録のみ。conditions_enteredは条件が不成立から成立した瞬間（初回成立も含む）。rule.once=trueは条件成立後の最初の実行だけ。
- 条件はposition_at_most/at_least,velocity_at_most/at_least,variable_at_most/at_least,distance_at_most/at_least。距離はcondition.nameの一意なオブジェクト名とのワールド距離、開始時のスナップショット。変数は数値のみ。軸X0/Y1/Z2。
- Action.targetはself/contact/name/tag。name指定ではtargetNameが唯一の名前、tag指定では同じオブジェクトタグの最大32対象。contactは現在の接触イベントの相手。名前重複や対象消失時はスキップ。遅延処理は開始時のIDへ固定し、同名の新しい対象へ差し替えない。
- 旧速度/加速度/移動/回転/スケール/位置の操作は、指定対象へ適用。scene変数操作はself指定で使う。set_visibleは描画コンポーネントだけの有効状態、set_activeはオブジェクト全体（valueは0/1）。自分を無効化するとタイマーも止まるため復帰は別のコントローラーで行う。
- create_objectはself指定でnameの空オブジェクトを生成する。clone_objectは指定対象を子なしで複製しnameの名前にする。生成位置は所有者のワールド位置にaxis方向へvalueだけオフセット。Prefabファイルを直接読み込まない。delete_objectは指定対象を最後に削除。生成は1更新16個、シーン全体4096個まで。
- change_sceneはself指定でnameのシーンへ遷移を要求。遷移先は実際にSceneListへ登録済みである必要がある。
- set_textは対象のTextRenderer/BitmapTextRendererへtextを設定。show_variable_textはnameの数値変数をtextの後に付けて表示。初期変数や描画コンポーネントが必要。
- followはreferenceの一意なオブジェクトのワールド位置にaxis方向へvalueのオフセットを加え、指定対象をそこへ移す。move_towardはreferenceへvalue単位/秒で近づく。親の変換は実行時に考慮する。衝突解決・経路探索はしない。
- tween_position/tween_rotation/tween_scaleは指定軸を現在値からvalueへduration秒で線形補間。位置とスケールはローカル、回転は度。tween_scaleは0より大きく100以下。同じ軸を他の動作で同時に制御しない。
- waitはduration秒待つ。待機・補間の完了後に次の動作を実行する。1ルールに実行中シーケンスは1件、実行中の再トリガーは捨てる。繰り返しevery_frameにwaitを付けると完了後の次フレームから再開する。単発演出にはstartかonceを使う。
- 古いplay_sound/stop_sound/set_volumeは対象の設定済みAudioSourceを使用。set_variable/add_variableはnameの数値変数を操作。translate/rotateは単位/秒・度/秒。set_*は絶対値。その他はイベントごと。任意コード、外部ファイル、物理接地やPrefab読込は使わない。
- 全Actionにtarget,targetName,reference,text,duration,name,axis,valueを入れる。使わない文字列は空、durationは1。全Ruleにbutton=0（必要時以外）、once=false（必要時以外）、variableは空（variable_changed以外）、tagは空（接触以外）、key=Space（必要時以外）、interval=1（timer以外）を入れる。
必要な22挙動:
1 Coinタグの相手に触れ始めたらscoreを1増やして接触相手を削除（所有者は生存）。
2 Space押下で名前Doorの表示を消す。
3 Enter押下で名前Doorの表示を出す。
4 開始時に名前Enemyのオブジェクトを無効にする。
5 Q押下でEnemyタグの全対象を有効にする。
6 マウス左押下で空オブジェクトGeneratedを生成。
7 マウス右押下で名前BulletTemplateを複製してBulletと名付け、所有者のワールドZ+1へ生成。
8 マウス左を離したら名前HUDのテキストをReleasedへ。
9 マウス中を押している間、所有者自身が名前Playerへ毎秒2で近づく。
10 所有者のUIButtonがクリックされたらStage1へ遷移要求。
11 Escape押下でTitleSceneへ遷移要求。
12 毎フレームscoreを名前HUDへ表示（prefixはScore: ）。
13 開始時に名前HUDへREADYを一度表示。
14 毎フレーム所有者が名前Playerへ追従し、ワールドY+3へ。
15 Playerとの距離が5以下を初めて満たした瞬間に、名前HUDへNEARを表示。
16 Playerとの距離が10以上なら、所有者がPlayerへ毎秒3で近づく。
17 Space押下で名前DoorのローカルXを1秒で5へ補間し、完了後に名前HUDへOPEN。
18 開始時、所有者のローカルY回転を2秒で90度へ補間。
19 E押下で所有者のXスケールを1秒で2へ補間。
20 開始時、一度だけ2秒待って設定済みの音を鳴らし、さらに1秒待ってTitleSceneへ遷移要求。
21 scoreが変わったとき名前HUDにprefix Score: で値を表示。
22 hpが0以下の条件を初めて満たした瞬間にTitleSceneへ遷移要求。
文の意味を実装以上に広げない。関係するオブジェクト名や数値を明示する。JSON以外を出力しない。
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
    if args.batch in ("interactions", "orchestration"):
        previous = json.loads((ROOT / "EditorTools/ShortTextDefaults.json").read_text(encoding="utf-8-sig"))
        document["entries"] = merge_entries(previous["entries"], document["entries"])
    candidate = OUTPUT / "ShortTextDefaults.candidate.json"
    candidate.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report = {"requestedModel": "claude-opus-5-5", "models": list(response.get("modelUsage", {})),
              "batch": args.batch, "phrases": len(document.get("entries", [])), "durationMs": response.get("duration_ms"),
              "costUsd": response.get("total_cost_usd")}
    (OUTPUT / "ShortTextGenerationReport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    print("Candidate: " + str(candidate))

if __name__ == "__main__":
    main()
