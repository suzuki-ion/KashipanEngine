# 短文挙動の拡張部品と対応範囲

重複して指定された「変数同士の比較・代入・計算」は1項目とし、51項目へ整理しています。以下は実行部品の対応表です。部品が存在しても、そのすべての日本語表現を理解できるわけではありません。標準学習辞書にある例文、またはユーザーが登録した短文とJSONレシピから利用します。

## 共通ルール

- `engine_command`動作のname/dataの詳細は `ShortTextExtendedCommands.json`（12）、`ShortTextRenderingCommands.json`（17）、`ShortTextPhysicsCommands.json`（15）、`ShortTextMediaCommands.json`（37）、`ShortTextSpatialCommands.json`（2）を参照。名前を数えた合計は83コマンド。共通ランタイムは15イベント・9条件・35動作。
- 汎用プロパティは `Component.member`、`Component[index].member`、末尾の `.x/.y/.z/.w` または `.r/.g/.b/.a` を指定。公開されている型付きメンバーのみ操作できます。未公開の内部データは任意に書き換えられません。
- 描画先、素材、音、Prefab、アニメーションなどの参照先は、プロジェクトに実在する必要があります。
- 連続した追従・レイアウト同期・物理移動は毎フレームのルールから実行します。1回の実行は1回の更新です。
- 実装の対応と、GUI・GPU・音・実際のゲーム操作の確認は別です。ビルドやheadlessテストだけを視覚・音響確認とは扱いません。

## 対応表

共通ランタイムは `engine_command`、`tween_property`、`parallel/repeat/branch/sequence_control/set_state` の動作を利用します。条件式は `expression` の `read/op/args` 木です。描画・物理・メディアのコマンド名は各実装へ直接対応します。

| # | 指定項目 | 実行経路 | 前提・限定範囲 |
|---|---|---|---|
| 1 | 文字列・ベクトル・色・オブジェクト参照の変数操作 | `variable.set/get`、`vector/color`式、汎用property | オブジェクト参照は安定UUID。生ポインターを永続化しない |
| 2 | グローバル変数の操作・変化検出 | `variable.*`の`scope:global`、`variable_changed`の`global:name` | Sceneのglobal variablesへ接続 |
| 3 | 変数同士の比較・代入・計算 | `variable.copy/calculate`、`add/sub/mul/div/min/max/clamp`式 | add/sub/mul/divはベクトル要素演算とスカラーbroadcast。型不一致やゼロ除算は拒否 |
| 4 | 等値・不等値・範囲内判定 | `eq/ne/between`式 | 数値と異なる型の比較は区別 |
| 5 | OR・NOT・条件の入れ子 | `and/or/not`式 | 木の深度最大8 |
| 6 | 対象の存在・有効状態・コンポーネント有無 | 共通対象・条件式、`object.exists/active` property | 不在の対象を無断生成しない |
| 7 | 親・子・階層からの指定 | `parent/children/descendants`対象 | 現在のScene内、子孫32回・対象32件以内 |
| 8 | ID指定・生成結果の参照 | `id/last_created`対象、適用時のlast_created解決 | Scene内の安定UUID |
| 9 | 最寄り・範囲内・複数候補 | `nearest/range/index`対象 | tag・radius・indexを指定、対象32件以内 |
| 10 | property取得・代入・同期・補間 | `property.set/sync/unsync`、`tween_property` | 型付き登録メンバー、Scriptの公開変数。syncは登録後毎フレーム自動更新、補間はシーケンスで進行 |
| 11 | テクスチャ・マテリアル切替 | `render.texture/material/pipeline` | 読込済素材。TextureSourceで対象固有の差替え |
| 12 | UV・画像切抜き・反転 | `render.uv` | SpriteRendererの正規化UV矩形。回転はラジアン |
| 13 | アンカー・ピボット・画面端・レイアウト同期 | `render.layout`、property | XY単位quad、root Transform、root無回転等倍Camera2D。毎フレーム実行で解像度追従 |
| 14 | 描画先・描画順・レイヤー | `render.target/order/layer` | レイヤーはBackground/World/UI/Overlayの優先度規約。衝突・カリングのレイヤーではない |
| 15 | 材質・shaderパラメータ | `render.material_parameter` | `shared:true`必須。extraは既存shaderのfloat/bool/vec4フィールドのみ。shader生成や素材ファイル保存はしない |
| 16 | ポスト効果の操作・補間 | `post.configure`、property補間 | 既存Effectの登録パラメータ。対象へのEffect追加はcomponent.add |
| 17 | テキスト色・透明度・font・size・書式 | `text.style/format`、property | TextRenderer。font変更は読込済font、数値書式の桁数上限あり |
| 18 | ホバー・ドラッグ・長押し時間・スクロール | physics adapter入力environment | UIButtonの範囲、マウス入力スナップショット。条件立上りでイベント化 |
| 19 | マウス位置・UI内・world座標 | 入力environment、`mouse_world` | world変換はCamera2D、UVは左下原点。UI矩形UVと画面全体UVは区別 |
| 20 | ゲームパッド・InputCommand | 入力environment、`input_command/gamepad_vibration` | 接続されたindex0..3。InputCommandは事前登録名 |
| 21 | CharacterController移動 | `controller_move` | 対応ColliderとSceneObjectCollider。毎フレーム移動予約 |
| 22 | 接地・壁・天井 | `physics_state`とphysics environment | CharacterControllerの前回移動結果 |
| 23 | RigidBody力・衝撃・状態 | `rigid_force/impulse/velocity/settings` | 初期化済2D/3D物理ボディ。2DはXY |
| 24 | 接触法線・侵入深さ | `collision_query`のresults配列 | collision_queryのresultをcontactsにすればresults.contacts.0.depth/normal.1を式で取得。接触イベント自体は相手IDの通知 |
| 25 | raycast・任意形状問い合わせ | `raycast/collision_query` | 3D rayは正確な有限ray。2Dは線分重なりで交点/fractionなし |
| 26 | 向きを合わせる | `look_at`、`spatial.direction/distance` | 2Dは+X、3Dは+Z。親回転をローカルへ変換 |
| 27 | 加減速移動・到達イベント | `accelerated_move`とresults.arrived条件 | 毎フレーム実行。到達条件の立上りで発火 |
| 28 | 経路・巡回・障害物回避 | `grid_path/waypoint_move` | 4近傍BFS、最大128×128、地点最大256（レシピの直接配列は64）。動的障害物には再計算が必要 |
| 29 | カメラ設定・演出 | `camera.configure`、property、`shake.*` | 投影値の補間・既存Shake。追従は既存Follow等と組合せ |
| 30 | Animator・KeyFrameAnimator再生制御 | `animator.* / keyframe.*` | 登録済clip/name、speed/time/loop |
| 31 | アニメ完了・途中時刻 | media environmentのfinished/time条件 | 初回から停止中は完了として扱わない。時刻条件の立上りを利用 |
| 32 | easing・曲線・最短角度 | `tween_property`と位置回転scale補間 | linear/ease_in/ease_out/ease_in_out/smoothstep、curve最大64samples、回転shortest |
| 33 | 複数軸・対象の補間group | `parallel`とベクトル`tween_property`・対象集合 | branches最大8、全branch完了まで待機 |
| 34 | shake・particle開始停止設定 | `shake.* / particles.*` | 既存Shake/ParticleSystem2D/3D。emit rate/count設定 |
| 35 | 並列・繰返し・回数・分岐 | `parallel/repeat/branch` | repeat1..32回、branches/actions最大8、有限ネスト |
| 36 | sequence取消・再開始・pause・置換 | `sequence_control` | rule0..15、cancel/pause/resume/restart/replace |
| 37 | cooldown・入力受付期間 | rule`cooldown/windowStart/windowEnd`、elapsed式 | cooldown最大3600秒。windowは開始からの秒数で入力受付のみ制限。入力の先行予約bufferは未実装 |
| 38 | オブジェクト単位state machine | `set_state`とrule`state` | オブジェクトのProgramMemory内状態を管理 |
| 39 | 音源指定・複数音源 | `audio.*`のindex/sound | 対象のAudioSourceをindex又は一意asset名で選択 |
| 40 | 音pause/resume/終了 | `audio.pause/resume`とaudio.finished | 既存AudioSource。完了は再生→停止の変化（自然終了と明示停止の厳密区別なし） |
| 41 | 音量補間・pitch・loop・空間音響 | `audio.configure`、AudioSource property補間 | pitchは半音、spatial/minDistance/maxDistance。実音確認は別途 |
| 42 | VideoSource・GifSource再生制御 | `video.* / gif.*` | 有効assetと対応コンポーネント |
| 43 | Prefabファイル生成 | `prefab.spawn` | 有効JSON、最大256node、参照ID再マッピング |
| 44 | 生成位置・向き・親・初期値 | `prefab.spawn`のpose/parent/initial | 指定親基準のローカル値。初期property不成立時は生成を取消 |
| 45 | object pool・再利用 | `pool.acquire/release` | 浅い複製、最大256。非active objectを再利用 |
| 46 | scene読込完了・常駐・引継ぎ | 開始イベント、`object.persist/restore`、global variables | root snapshotをScene間復元。同じnative objectを存続させる方式ではない |
| 47 | save/load・設定永続化 | `save.write/read` | ローカルShortTextSaves、型付きglobal変数。slot名制限 |
| 48 | Tilemapセル・地形・tile操作 | `tilemap.get/set/solid/resize/clear` | 対象TilemapRendererと既存tile ID、最大1048576cell。経路問い合わせはgrid_path |
| 49 | Window・ScreenBuffer・Camera生成設定 | `render_target.configure / camera.configure`、`component.add` | 現在対象へcomponent生成。新objectは既存生成部品と組合せ。実GPU/window初期化が必要 |
| 50 | component追加削除設定 | `component.add/remove/configure` | 登録型、公開member。Transformと実行中ShortTextBehaviorの削除禁止 |
| 51 | script関数・イベント送受信 | `script.call / event.emit`、変化条件 | script関数は引数なし、イベントはScene内serial変数の変化で受信 |

## 検証方法

同じ構成のエンジンを先にビルドし、Visual Studioのx64開発環境から実行します。production `.obj` を使い、テストmainだけをコンパイル・リンクします。

```powershell
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextRenderingRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextPhysicsRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextMediaRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextPropertiesRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextExtendedRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextExtendedSceneRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextSpatialRegression.cpp
./Project/Tests/RunShortTextDomainRegression.ps1 -SourceFile ShortTextBehaviorSceneRegression.cpp
```

生成projectと実行ファイル名は実行ごとに一意です。元のエンジンproject・production objectは変更しません。作成したprojectとexe/pdb/ilk/exp/libは終了時に削除します（実行直後のファイルロックに備えて再試行し、削除できない場合は警告のみ）。

