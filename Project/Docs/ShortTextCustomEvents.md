# ユーザー定義イベントとゲーム部品（短文対応）

シーンごとに名前と型付き引数を持つイベントを定義し、C++・AngelScript・Inspector・短文から同じ定義で送信／受信できます。
実行時に AI や通信は使いません。短文は有限の文法で解釈し、解釈できない文は何も変更しません。

文字列の引数は引用符で囲めます。例えば `Greetingイベントを全体に、textを「Hello world、Ｆｏｏ　bar」として送る` は、引用符内の空白・全角文字・句読点を保持します。文法部分だけを空白・全角正規化します。不正なUTF-8や整数のオーバーフローは送信前に拒否します。

## 構成

| 種類 | クラス | 役割 |
| --- | --- | --- |
| シーンコンポーネント | `GameEventRouter`（`Scene/Components/GameEventRouter.h`） | イベント定義（保存される）、送信キュー、購読、配送、診断 |
| オブジェクトコンポーネント | `GameEventListener`（`Objects/Components/GameEventListener.h`） | きっかけ（イベント受信／再生開始／接触／キー）＋条件＋動作の受信処理 |
| 純粋データ | `GameEvents`（`Utilities/GameEvents.h`） | 名前規則、引数スキーマ、型付きペイロードの検証、JSON |
| スクリプト | `GameEvent` 型と `EmitEvent` など（`ScriptBindings.cpp`） | 送信・購読・定義・型付き getter |
| ゲーム部品 | `Health` `ContactDamage` `Pickup` `Inventory` `Lifetime` `Spawner` `Interactable` `Respawner` `Checkpoint` `Dialogue` `AnimationStateSwitcher`（`Objects/Components/GameplayComponents.h`） | 10 カテゴリの部品。イベントを送り、`GameEventListener` の「部品の命令」で操作される |
| 短文 | `ShortTextEventRequest`（文法）、`ShortTextEventPlan`（エディタ計画・Undo） | 短文から定義・受信処理・部品を作成 |

## イベント定義

- 名前: 1〜64 バイトの UTF-8。空白・制御文字・全角空白と `. : , " ' \ / [ ] { } # @` は使えません。
  **大文字小文字を区別せずに一意**です（`GoalReached` と `goalreached` は同じイベント。保存された綴りで表示）。定義は 1 シーン 256 個まで。
- 引数（16 個まで）: `bool` / `int`（int32 の整数）/ `number`（有限の数値、|x|≤1e9）/ `string`（1024 バイト、制御文字なし）/
  `vector2` / `vector3` / `vector4`（数値の配列）/ `color`（0〜1 の 4 要素）/ `object`（オブジェクト UUID 文字列。空文字は「オブジェクトなし」）。
- `required` の引数が無い送信、未定義の引数、型の不一致は**送信されません**（キューに入りません）。省略可能な引数は省略値があれば補われ、無ければペイロードに含まれません。
- ペイロード全体は 4096 バイトまで。生ポインタは保存・送信しません（オブジェクトは UUID）。

### 保存形式（シーン JSON の GameEventRouter の customData）

```json
{
  "definitions": [
    {"name": "Damage", "description": "", "args": [{"name": "amount", "type": "number", "required": true}],
     "shortTextManaged": true, "sourceText": "Damageイベントを作成、amountは数値"}
  ],
  "limits": {"maxQueue": 1024, "maxPerUpdate": 256, "maxDepth": 8}
}
```

Inspector（シーンコンポーネント）で名前・説明・引数・型・必須・省略値を編集でき、編集は Undo/Redo に記録されます。

## 送信と配送

- 送信先: シーン全体 / オブジェクト UUID / 一意な名前（送信時に UUID へ固定。無い・重複は拒否）/ タグ（該当する全オブジェクト）。
- 配送はキュー経由で、シーンの更新中（再生中）に 1 件ずつ行います。同じイベントが同じ更新で何回送られても、それぞれ別に配送されます。
- 制限: 1 更新の配送数（既定 256。残りは次の更新）、キュー長（1024）、連鎖の深さ（受信処理の中で送ったイベントは深さ +1、既定 8 を超えると拒否）。
  A→B→A の循環は深さの上限で止まり、フレームは止まりません。拒否・打ち切り・受信処理の例外は Inspector の「診断」に表示されます。
- 受信者は ComponentRef（安定 UUID）で保持し、配送の直前に毎回解決します。削除済みは購読を外し、無効のものには配送しません。
  受信処理の中で自分や他のオブジェクトを削除・無効化・購読し直しても安全です（配送先は先に確定、削除の実体は配送後）。
- ルーターはシーンごとに 1 つです。シーンを切り替えると購読ごと破棄され、古いシーンへは配送しません。ルーターを削除・置き換えると
  `GameEventListener` は世代番号の変化を見て購読し直します（スクリプトの購読は Awake / Start で作り直してください）。
- 旧 `event.emit`（シーン変数 `__shortTextEvent.NAME` の連番）はそのまま動きます。同名の定義がある場合だけ、ルーターへも 1 回送ります
  （引数が不正ならどちらも変えません。ルーターは連番を変更しないので二重配送しません）。

## GameEventListener（受信処理）

各受信処理: ID、きっかけ（イベント受信 / 再生開始 / 接触 / キー）、送信元タグの条件、受信引数の条件（`== != < <= > >=`、4 個まで）、
一度だけ、クールダウン、動作（8 個まで）。動作の種類:

| 動作 | 例 | 引数 |
| --- | --- | --- |
| プロパティを設定 | `Transform.translate_.x` | `value` |
| 実行部品 | `keyframe.play`（`name`）、`animator.play` など | 実行部品の引数 |
| イベントを送信 | `Damage` を全体／UUID／名前／タグ／`$sender` へ | 送るイベントの引数 |
| スクリプト関数 | `OnGoal()`、`Take(float)`、`void OnDamage(GameEvent@)` | 0〜4 個（型を変換、変換できなければ呼ばない） |
| 部品の命令 | `Health.damage`、`Inventory.add`、`Dialogue.start`、`Spawner.spawn`、`Respawner.respawn` | 部品ごと |

各引数は「固定値」「受信した引数」「送信元 UUID」「イベント名」から選べます（受信した値は型付きのまま渡ります）。
実行部品は受信処理ごとの専用メモリで実行され、受信処理・オブジェクトの間で状態は共有されません。毎フレームの継続実行が前提の
`accelerated_move` / `waypoint_move` / `property.sync` / `property.unsync` は受信処理では使えません（設定時に拒否）。

## スクリプト API

```angelscript
class Goal : ScriptComponentBehavior {
    void Awake() {
        SubscribeEvent("Damage", "OnDamage");             // 受信関数は void 関数(GameEvent@)
    }
    void OnDamage(GameEvent@ e) {
        float amount = e.GetFloat("amount");               // GetInt / GetFloat / GetBool / GetString / GetVector2/3/4 / Has / GetPayload()
        string from = e.sender;                            // 送信元 UUID（無ければ ""）、e.name、e.depth
    }
    void Fire() {
        Json j; j.SetFloat("amount", 7);
        if (!EmitEvent("Damage", j)) { string why = GetLastEventError(); } // 未定義・型の不一致などの理由
        EmitEventTo("Damage", j, "tag", "Enemy");          // "broadcast" / "object"(UUID) / "name" / "tag"
    }
}
```

`DefineEvent(name, args, description)`（args は `[{"name":"amount","type":"number","required":true}]` 形式の Json）、`HasEventDefinition`、
`UnsubscribeEvent` もあります。`GameEvent` はメッセージのコピーを所有する参照カウント型で、ハンドルを保持し続けても安全です。
受信関数は配送キューから呼ばれ、同じ呼び出しの中で再入しません。任意の C++ コードの評価はしません。

## 短文（「短文の挙動（試験）」ウィンドウ）

学習した言い回しが最優先で、次にこのイベント文法、次に標準例文（486 文）、最後に「〇〇の〇〇」文法です。
このイベント文法は標準例文 486 文のどれも解釈しないこと（横取りしないこと）をテストで確認しています。
解釈すると、追加・設定される内容と「自動では設定しない必要事項」をプレビューし、「適用」で 1 回の Undo にまとめて実行します。
計画・プレビューだけではシーンを変更しません。

| 文（数値・名前は変更可） | 生成される構成 |
| --- | --- |
| `GoalReachedイベントを作成` | GameEventRouter（無ければ追加）に定義 `GoalReached()` |
| `Damageイベントを作成、amountは数値`（`powerは整数`、`msgは省略可能な文字列`、`dirはVector3`、`whoはオブジェクト` など） | 定義 `Damage(amount: 数値 必須)` |
| `PlayerがGoalに触れたらGoalReachedを発火` | Player の GameEventListener に「Goal（名前、無ければタグ）に触れたら GoalReached を送る」。Collider・SceneObjectCollider が無ければ必要事項として表示 |
| `GoalReachedを受け取ったら1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動` | KeyFrameAnimator に自動再生しない本物のキーフレーム（`ShortText.OnGoalReached.Transform.translate_.y`）＋受信処理 `keyframe.play` |
| `Damageイベントを対象Enemyに、amountを10として送る` | 選択中のオブジェクトの受信処理（きっかけ無し＝再生開始時に 1 回）で Enemy の UUID へ `Damage{amount:10}` |
| `Damageイベントで対象Enemyに10を渡す` | 引数が 1 つのイベントへの同じ送信（`タグEnemyに` でタグ宛て） |
| `GoalReachedを受け取ったらOnGoal関数を呼ぶ` / `…OnDamage関数をamountを渡して呼ぶ` / `…OnDamage関数にイベントを渡して呼ぶ` | 受信処理（スクリプト関数）。ScriptComponent は作らず必要事項に表示 |
| `Damageを受け取ったら体力をamountだけ減らす`（`体力をamountだけ回復`、`会話を開始` など） | 受信処理（部品の命令 `Health.damage(amount=受信.amount)`）。Health が無ければ追加 |
| `Eキーを押したらGoalReachedを発火` / `Pingを受け取ったらPongを送信元に送る` | キーのきっかけ / 送信元への返信 |

拒否（何も変更しない）: 未定義のイベント、定義に無い引数、型の合わない値、必須引数の不足、同名オブジェクトが複数ある名前指定、
名前とタグが別オブジェクトに一致、存在しない相手、手作りの同名定義と異なる引数、手作りの単一部品との衝突、再生中の適用。

管理: 短文で作った定義・受信処理・部品は `shortTextManaged` と生成元の文を保存し、同じ文（同じきっかけと動作）の再適用は重複させず更新します。
手作りの定義・受信処理・部品・クリップは削除も上書きもしません。他のオブジェクト（例: 受け取り手の Inventory、鍵の Pickup）への変更も同じ Undo に含まれます。

## 10 カテゴリの部品と短文

| # | 部品 | 短文の例 | 主な設定と動作 |
| --- | --- | --- | --- |
| 1 | `Health` + `ContactDamage` | `体力を100にして、Enemyに触れたら10ダメージ` / `Enemyに触れたら10ダメージを与える` | 最大・現在（0〜最大）、無敵時間（既定 0.5 秒、重複ダメージ抑制）、整数のみ、死亡時（何もしない/無効化/削除）、ダメージ/回復/変化/死亡イベント（amount/current/max/previous/sender）。接触は相手ごとのクールダウン |
| 2 | `Pickup` | `Playerに触れたらコインを1増やして消える` | 受け取り手タグ、アイテムID、個数、一度だけ、取得後（残す/無効化/削除）、取得イベント。受け取り手の Inventory へ追加（または整数シーン変数）。受け取り手が 1 体だけなら Inventory を追加、無ければ必要事項（別の Player は作らない） |
| 3 | `Lifetime` | `生成されてから3秒後に消える` / `5秒後に無効化する` | 再生中だけ進む。削除／無効化（プール返却）、期限イベント。再有効化・Spawner の再利用で 0 秒から。不正な dt（NaN・負・10 秒超）は無視 |
| 4 | `Spawner` | `2秒ごとにEnemyを生成し、最大5体にする` | 既存オブジェクトを UUID で参照（無い・重複・自分や親・Spawner を含むものは拒否、アセットは作らない）。最大同時数、1 更新の生成数、シーン全体 4096 体（テンプレートの子孫を含めて判定）、無効化された生成物の再利用、生成/上限イベント |
| 5 | `Interactable` | `近づいてEキーを押したら扉を開く` / `…Openを再生する` / `…Openedを発火` / `…会話を開始` | 相手タグと距離、キーまたは InputCommand のアクション、必要アイテム（消費の有無）、KeyFrameAnimator / Animator のクリップ、Dialogue、実行/拒否イベント。開く方法が無ければ「Open クリップかイベント」を必要事項として表示 |
| 6 | `Respawner` + `Checkpoint` | `触れた場所を復活地点にして、落下したら戻る` / `…Yが-20より下になったら戻る` / `…倒れたら戻る` | 再生開始位置が最初の地点、タグ Checkpoint に触れると更新（Checkpoint のオフセット付き）、落下の高さ・Health 0 で復活、ワールド座標で戻し（親があればローカルへ変換）、Velocity を 0、Health を最大へ、復活イベント |
| 7 | `Inventory` | `インベントリを追加` / `鍵を取得し、鍵を持っていたら扉を開ける` | アイテムID→個数、種類数・1 種類の上限、add/remove/consume/has（不足や上限超えは何も変えない）、増減イベント、グローバル変数 `__inventory.<保存キー>` への保存/読込。鍵の文は扉に必要アイテム付き Interactable、「鍵」という名前のオブジェクトに Pickup を設定 |
| 8 | `Dialogue` | `話しかけたら文章を順番に表示する` | 行（話者・文章・次の行）と選択肢（4 個まで、行き先）。開始/進める/選ぶ/終了、表示先 TextRenderer / BitmapTextRenderer、進めるキー・1〜4 キー、開始/行/選択/終了イベント。**文章は生成しません**（未入力なら必要事項） |
| 9 | `AnimationStateSwitcher` | `移動中はWalk、停止中はIdleを再生する` | 速度（Velocity、無ければ位置の変化）としきい値・優先度で状態を選び、変わった時だけ KeyFrameAnimator / Animator を再生（同じ状態は再生し直さない）、遷移イベント。クリップが無ければ推測せず必要事項 |
| 10 | 引数付きイベント | `Damageイベントを対象Enemyに、amountを10として送る` + `Damageを受け取ったら体力をamountだけ減らす` | 型付きペイロードの値がそのまま部品の命令・スクリプト関数の引数になる |

各部品は Inspector で編集でき（編集は Undo 対象）、JSON 保存・複製・シーン/オブジェクト JSON（プレハブ）に含まれ、再生停止で編集時の値へ戻ります。

## テスト

- 純粋: `Project/Tests/RunShortTextEventRequestRegression.ps1`（スキーマ・型付きペイロード・文法・10 カテゴリの値検証・標準 486 文を横取りしないこと）
- ネイティブ: `Project/Tests/ShortTextCustomEventRegression.cpp`（`RunShortTextDomainRegression.ps1 -SourceFile ShortTextCustomEventRegression.cpp`）
