# パズルゲーム パラメータ調整ガイド

TestScene の ImGui ウィンドウ「Puzzle Game Config」で調整できるパラメータの一覧です。

---

## NPC Settings

| パラメータ名 | 説明 |
|---|---|
| NPC Mode | NPCモードの ON/OFF。ON にするとプレイヤー2が NPC として自動操作される |
| NPC Difficulty | NPCの難易度。Easy / Normal / Hard から選択 |

---

## Config（基本設定）

| パラメータ名 | 型 | デフォルト値 | 範囲 | 説明 |
|---|---|---|---|---|
| Stage Size | int | 6 | 3〜8 | ステージの大きさ（n × n マス） |
| Panel Scale | float | 80.0 | 16〜128 | 1枚のパネルの表示サイズ（ピクセル） |
| Panel Gap | float | 8.0 | 0〜16 | パネルとパネルの間の隙間（ピクセル） |
| Panel Type Count | int | 4 | 2〜8 | パズルパネルの種類数。多いほど揃えにくくなる |
| Panel Move Easing | float | 0.15 | 0.01〜1.0 | パネルが移動する際のアニメーション時間（秒） |
| Panel Clear Easing | float | 0.3 | 0.01〜2.0 | パネルが消える際のアニメーション時間（秒） |
| Panel Spawn Easing | float | 0.15 | 0.01〜1.0 | パネルが新しく出現する際のアニメーション時間（秒） |
| Cursor Easing | float | 0.1 | 0.01〜1.0 | カーソルが移動する際のアニメーション時間（秒） |
| Normal Min Count | int | 3 | 2〜8 | 「ノーマル消し」として認識される最低パネル数 |
| Straight Min Count | int | 5 | 3〜8 | 「ストレート消し」として認識される最低パネル数（Normal Min Count より大きい値にすること） |

---

## Battle Settings（対戦設定）

| パラメータ名 | 型 | デフォルト値 | 範囲 | 説明 |
|---|---|---|---|---|
| Time Limit | float | 30.0 | 1〜60 | 1ターンの制限時間（秒）。時間切れで自動的にパネル消去が発動する |
| Normal Lock Time | float | 1.0 | 0〜30 | 「ノーマル消し」1回あたりの基本ロック時間（秒） |
| Straight Lock Time | float | 3.0 | 0〜30 | 「ストレート消し」1回あたりの基本ロック時間（秒） |
| Cross Lock Time | float | 5.0 | 0〜30 | 「クロス消し」1回あたりの基本ロック時間（秒） |
| Square Lock Time | float | 15.0 | 0〜30 | 「スクエア消し」1回あたりの基本ロック時間（秒） |
| Combo Lock Mult | float | 2.0 | 1〜10 | コンボ発生時のロック時間倍率。2コンボ以上の場合、`倍率^(コンボ数-1)` がロック時間に掛けられる |
| Break Lock Mult | float | 2.0 | 1〜10 | 「ブレイク」（ボード上の全パネルを消した状態）発生時のロック時間倍率 |
| Remain Time Lock Bonus | float | 0.5 | 0〜5 | 制限時間を途中でスキップした場合、余った秒数 × この値がロック時間に加算される |

### ロック時間の計算式

```
基本ロック時間 = (ノーマル数 × Normal Lock Time)
              + (ストレート数 × Straight Lock Time)
              + (クロス数 × Cross Lock Time)
              + (スクエア数 × Square Lock Time)

コンボ倍率 = Combo Lock Mult ^ (コンボ数 - 1)    ※2コンボ以上の場合
ブレイク倍率 = Break Lock Mult                     ※ブレイク成立時のみ

最終ロック時間 = (基本ロック時間 × コンボ倍率 × ブレイク倍率)
              + (残り制限時間 × Remain Time Lock Bonus)
```

---

## Garbage Settings（お邪魔パネル設定）

| パラメータ名 | 型 | デフォルト値 | 範囲 | 説明 |
|---|---|---|---|---|
| Moves Per Garbage | int | 5 | 1〜30 | 自分のボードにお邪魔パネルが自然出現するまでの移動回数 |
| Attack Garbage Mult | float | 0.5 | 0〜3 | 攻撃時の全体的なお邪魔パネル倍率 |
| Inactive Decay Interval | float | 1.0 | 0.1〜10 | 使用していないステージのお邪魔パネルが1個自然消滅するまでの秒数。小さいほど早く回復する |
| Normal Garbage Count | float | 1.0 | 0〜20 | 「ノーマル消し」1回あたりの基本お邪魔パネル出現量 |
| Straight Garbage Count | float | 3.0 | 0〜20 | 「ストレート消し」1回あたりの基本お邪魔パネル出現量 |
| Cross Garbage Count | float | 5.0 | 0〜20 | 「クロス消し」1回あたりの基本お邪魔パネル出現量 |
| Square Garbage Count | float | 9.0 | 0〜20 | 「スクエア消し」1回あたりの基本お邪魔パネル出現量 |
| Combo Garbage Mult | float | 1.5 | 1〜5 | コンボ発生時のお邪魔パネル出現量倍率。2コンボ以上の場合、`倍率^(コンボ数-1)` が出現量に掛けられる |

### お邪魔パネル出現量の計算式

コンボチェーン中（連鎖が続いている間）、お邪魔パネルの出現量は蓄積され続けます。最終的な個数は小数点以下を切り捨てた値になります。

```
今回の基本出現量 = (ノーマル数 × Normal Garbage Count)
               + (ストレート数 × Straight Garbage Count)
               + (クロス数 × Cross Garbage Count)
               + (スクエア数 × Square Garbage Count)

コンボ倍率 = Combo Garbage Mult ^ (コンボ数 - 1)   ※2コンボ以上の場合

今回の出現量 = 基本出現量 × コンボ倍率

蓄積出現量 += 今回の出現量   （コンボチェーン中は加算し続ける）

最終お邪魔パネル個数 = floor(蓄積出現量)   （小数点以下切り捨て）
```

---

## Defeat Settings（敗北条件）

| パラメータ名 | 型 | デフォルト値 | 範囲 | 説明 |
|---|---|---|---|---|
| Defeat Collapse Ratio | float | 0.7 | 0.1〜1.0 | 敗北とみなす崩壊度の閾値（0.0〜1.0）。両方のボードの崩壊度（お邪魔パネルの割合）がこの値以上になると敗北 |

---

## Colors（色設定）

| パラメータ名 | 説明 |
|---|---|
| Panel Color 1〜8 | 各パズルパネルタイプの色（RGBA）。Panel Type Count の数だけ表示される |
| Stage BG Color | ステージ背景の色（RGBA） |
| Cursor Color | カーソルの色（RGBA） |
| Lock Color | ロックオーバーレイの色（RGBA） |
| Garbage Color | お邪魔パネルの色（RGBA） |
| Garbage Warning | お邪魔パネル出現予告の色（RGBA） |

---

## ボタン

| ボタン名 | 説明 |
|---|---|
| Save Config | 現在のパラメータを JSON ファイルに保存する |
| Load Config | JSON ファイルからパラメータを読み込む |
| Restart | 現在の設定でシーンを再起動する |
