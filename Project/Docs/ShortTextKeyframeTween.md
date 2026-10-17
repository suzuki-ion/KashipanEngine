# 短文から KeyframeAnimation を生成する（一回だけの変化 / Tween）

短文の挙動ウィンドウ（**試験機能 → 短文の挙動**）で「N秒かけて … AからBまで <イージング>で 移動」と入力すると、
本物の `KeyframeAnimation`（キー：0秒=A＋指定イージング、N秒=B）を作り、`KeyFrameAnimator` のアニメーションとして登録します。
キーはシーン内（エントリへの埋め込み）に保存されるため、外部の json ファイルは作りません。解釈はローカルの文法だけで行い、実行時のAI・通信は使いません。

## 入力できる文の例

| 入力 | 生成されるもの |
| --- | --- |
| 1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動 | KeyFrameAnimator（無ければ追加）に `ShortText.Transform.translate_.y`：0秒=0（EaseInOutCubic）→ 1秒=4 |
| Transformの移動Y軸を0から4まで1秒かけてEaseInOutCubicで移動 | 上と同じ（「N秒かけて」は文頭でも範囲の後でもよい） |
| 2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転 | `rotate_.z`：キーは度（0→90）で保存、valueScale=π/180 でラジアンへ変換 |
| 1秒かけてTransformのスケールX軸を1.0fから2.0fまでLinearで変更 | `scale_.x`：1→2（Linear） |
| 1秒かけてBoxColliderのサイズX軸を1.0fから3.0fまでEaseInOutCubicで変更 | BoxCollider（無ければ既定値で追加）＋ `size_.x`：1→3 |
| 1秒かけてTransformの移動Y軸を4から0までEaseInCubicで移動 | 減少方向も可 |
| 1秒かけてTransformの移動Y軸を2から2までLinearで移動 | 同じ値（保持）も可 |

- 数値の末尾 `f`/`F` は任意。符号・小数・空白・全角数字（`１．５秒かけて` など）も使えます。項目・軸の別名は「〇〇の〇〇」文法と同じです（移動/位置/回転/スケール/サイズ/中心、`translate_` などの正式名）。
- イージングは `Utilities/MathUtils/Easings.h` の正式名（大文字小文字は無視）：Linear、EaseIn/EaseOut/EaseInOut/EaseOutIn × Quad・Cubic・Quart・Quint・Sine・Expo・Circ・Back・Elastic・Bounce（41種）。
  イージングを省略すると **Linear（既定）** になり、プレビューに「イージング指定なしのため既定」と表示します。
- 動詞は 移動/回転/変更/変化/拡縮/拡大/縮小/補間/アニメーション（＋させる/して など）。

## 再生の仕方

- 編集中の値は変更しません。再生（Play）開始フレームで必ず開始値を書き込み、N秒で終了値に到達して**1回だけ**で止まり、以後は終了値を保持します（loop=false、playOnStart=true）。
  初期値が何であっても（例：Y=9）値はローカル絶対値で 0→4 になり、他の成分（X/Z）は変わりません。停止（Stop）すると再生前の値へ戻ります。
- `KeyFrameAnimator::Play(name)` で再び先頭から、`Stop(name)` でその位置のまま止まります。

## 保存形式（KeyFrameAnimator の埋め込みエントリ）

```json
{ "name": "ShortText.Transform.translate_.y", "jsonPath": "", "playbackSpeed": 1, "timeOffset": 0,
  "valueScale": 1, "valueOffset": 0, "loop": false, "playOnStart": true,
  "bindings": [ { "componentType": "Transform", "componentIndex": 0, "parameterName": "translate_", "channel": 1, "isScriptVariable": false } ],
  "embeddedAnimation": { "keyframes": [ { "time": 0, "value": 0, "ease": "EaseInOutCubic" }, { "time": 1, "value": 4, "ease": "Linear" } ] },
  "shortTextManaged": true, "sourceText": "1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動" }
```

- `embeddedAnimation` があるエントリはキーをシーンに保持します（`jsonPath` は使いません）。無いエントリは従来どおり jsonPath のファイルを読み込みます。
  ファイル参照のエントリの保存内容は従来と同じで、追加の項目は書き出しません。
- イージングは「そのキーから次のキーへ」の補間です（最後のキーのイージングは未使用）。
- API：`AddAnimationData(name, json)` / `SetAnimationData(name, json)`（KeyframeAnimation::SaveToJson 形式、不正なら変更なし）、
  `IsEmbedded`、`GetAnimationData`、`SetShortTextSource`、`IsShortTextManaged`、`GetBindings`、`SaveAnimationEntryToJson`。`SetJsonPath` を呼ぶとファイル参照へ戻ります。
- Inspector：埋め込みエントリはキー一覧（時刻・値・次のキーへのイージング）を表示・編集・追加・削除できます。ファイル参照のエントリは「キーをシーンへ埋め込む」で読み込み済みのキーを複製して埋め込みにできます（元ファイルは変更しません）。
  短文由来のエントリには生成元の文章を表示し、valueScale が π/180 のときは「度→ラジアン」と表示します。

## Undo と置き換え

- 適用は1つの `CompositeCommand` で、1回の Undo/Redo で戻せます。解釈・プレビュー・コマンド生成だけではシーンを変更しません。
- 同じ項目・同じ成分への再適用は、短文由来のエントリだけを（名前を保ったまま）更新します。KeyFrameAnimator やエントリは増えません。
- 手作りのアニメーション（`shortTextManaged` でないエントリ）は削除・変更しません。同じ項目へ書き込んでいる場合はプレビューで注意を表示します。
- 同じ項目を動かす短文由来の `PropertyMotion`（往復）や旧方式の `ShortTextBehavior`（上下/左右の往復・移動・回転）は、Undo 可能な形で削除して置き換えます。
  逆に、往復・「Nにする」・旧方式レシピを同じ項目へ適用すると、短文由来のキーフレームエントリを削除します。別の項目・別の軸の動作はそのまま残ります。

## 拒否（シーンは変更しない）

所要時間なし（一回変化は「N秒かけて」が必須）・0以下・0.01秒未満・3600秒超・二重指定、未知のイージング・イージング二重指定、
往復と「N秒かけて」/イージングの混在、「Nにする」と「N秒かけて」の混在、周期・なめらか指定との混在、部分的にしか解釈できない文、
未知のコンポーネント/項目、読み取り専用、Vector の軸未指定、Vector2 への Z 軸、±10000 を超える値・非有限値、
サイズ/スケールなど正の値が必要な項目の0以下、および正の値が必要な項目への Back/Elastic 系（範囲外へはみ出すため負の値を書く恐れがある）。

`KeyframeAnimation::LoadFromJson` は厳密になりました：各キーの time（0以上の有限値）・value（有限値）が必須、ease は省略時 Linear・指定時は正式名のみ。
不正なデータは false を返し、以前のキー列を保持します。KeyFrameAnimator の読み込みも型・範囲を検証し、不正な場合は以前の状態を保持します。

## 検証

- 純粋：`Project/Tests/RunShortTextPropertyRequestRegression.ps1`（Tween の文・時間・イージング・拒否、旧文法の互換）
- ネイティブ：`Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextKeyframeRegression.cpp`
  （実 KeyframeAnimation::Evaluate＝0/0.25/2/3.75/4、KeyFrameAnimator 追加・再適用・手作り保持・Undo/Redo・再生・終了保持・停止/再開・回転の度→rad・BoxCollider 自動付与・置換・厳密読込・ファイル互換・Clone・保存/復元）
