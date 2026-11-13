# JobHuntingGameのロード画面

`Assets/Scenes/LoadingScene.scene`を追加。通常のシーンとして編集でき、黒背景と右下の白い回転アイコンだけを表示する。画像アセットは追加せず、円形に配置した12本のRect2Dスプライトを親のKeyFrameAnimatorで時計回りに1秒1回転させる。

## 遷移先

| 元の操作 | 遷移 |
|---|---|
| タイトルの開始・続きから | TitleScene → LoadingScene → CityScene |
| 街から遠征先選択 | CityScene → StageSelectScene（従来どおり） |
| 遠征先選択から出発 | StageSelectScene → LoadingScene → ExpeditionScene |
| 遠征先選択から街へ戻る | StageSelectScene → LoadingScene → CityScene |
| 遠征から街へ帰還 | ExpeditionScene → LoadingScene → CityScene |
| 街の設定反映・初期化による再読み込み | CityScene → LoadingScene → CityScene |

共通の`TownTransition.Begin(target)`にロード対象名を渡した場合だけ、従来のフェードアウト後に`ChangeSceneWithLoading(target, "LoadingScene")`を呼ぶ。引数なしの`Begin()`は従来の`ChangeToNextScene()`を使う。未登録の旧ゲーム用スクリプトや他のシーンは変更していない。

## 見た目と設定

- `LoadingCamera`: ウィンドウサイズに追従するCamera2D。
- `LoadingBackground`: 黒のスプライト。カメラの中央に追従し、スクリプトでカメラの幅・高さに合わせる。
- `LoadingSpinner`: 右下を基準に内側48pxの中心位置へ追従。アイコン外径は約44px。
- `LoadingSpoke0`〜`11`: 親の回転に追従する白いスプライト。濃淡を付け、回転方向が分かるようにしている。
- `LoadingController`: 最低表示時間0.5秒。準備完了後にアイコンを0.12秒で消し、準備したシーンを有効化する。背景は黒のまま、遷移先の既存フェードインへつなぐ。
- ウィンドウ名は他のLightTownシーンと一致させ、従来のウィンドウ引き継ぎを利用する。

最低表示時間とアイコンの終了時間は、LoadingControllerのScriptComponentのシリアライズ値で編集できる。LoadingSceneを単独で再生した場合は遷移先が指定されていないため、アイコンを表示し続ける。読み込みに失敗した場合も表示を維持し、原因を一度ログへ出力する。

## 検証（2026-10-30）

- `EngineTests/RunJobHuntingLoadingRegression.ps1`: 実際のスクリプトとシーンを読み、街・遠征へのロード挿入、ステージ選択への直接遷移、KeyFrameAnimatorの回転、親子座標を51項目で確認。
- `EngineTests/RunSceneLoadingRegression.ps1 -Configuration Release`: 54項目通過。Releaseで存在しない編集専用コンポーネントとその子孫を、構築・型検証の前に除外する回帰テストを含む。
- 隔離した実際のReleaseゲームで、Title → Loading:City → City → StageSelect → Loading:Expedition → Expedition → Loading:City → Cityを完走し、正常終了。元のゲームの保存データは使わず、QAコピー内だけに保存した。
- 画像出力用のバッファとカメラ・描画先の追加はQAコピーだけに実施。同じカメラ寸法で出力した2枚ずつのPNGから、黒背景、右下のアイコン、回転による画像の変化を確認。1280×720の非黒領域は`(1210, 650)〜(1254, 694)`。
- computer-useのウィンドウキャプチャは`0x887A0005`で失敗したため、画面キャプチャの代わりにエンジンの画像出力を確認した。

検証結果・ログ・画像は`Generated/JobHuntingLoading/`。QA用の長めの表示時間、経路制御スクリプト、画像出力用オブジェクトは本番プロジェクトに追加していない。

実行検証中に、Releaseの段階読み込みが編集専用の`SceneViewCameraSettings`を不明なコンポーネントとして拒否する問題が見つかったため、`ScenePreparation.cpp`の除外タイミングを修正した。
