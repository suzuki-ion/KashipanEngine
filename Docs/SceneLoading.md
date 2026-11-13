# ロード専用シーンを挟むシーン遷移

通常のシーンをロード専用シーンとして使い、そのシーンのスクリプト・描画・入力・音声・物理更新を実行しながら、遷移先を準備できる。Release起動時の軽量スプラッシュとは別の仕組みで、2D限定ではない。

## 最小の使い方

1. エディターで通常のシーンを作り、必要なウィンドウ、カメラ、表示物、KeyFrameAnimator、スクリプトなどを配置する。
2. シーンリストに`LoadingScene`と遷移先の`Stage1`を登録する。
3. 遷移元のスクリプトで呼び出す。

```angelscript
bool accepted = GetScene().ChangeSceneWithLoading("Stage1", "LoadingScene");
```

4. ロード専用シーンのスクリプトから準備完了を確認し、終了演出や入力待ちなどが終わった時点で切り替える。

```angelscript
void Update() {
    Scene@ scene = GetScene();
    float progress = scene.GetSceneLoadProgress();
    string error = scene.GetSceneLoadError();
    if (error.length() > 0) {
        // エラーを表示し、再試行またはメニューへ戻る。
        return;
    }
    if (scene.IsSceneLoadReady() && IsMyOutroFinished()) {
        scene.ActivateLoadedScene();
    }
}
```

`IsMyOutroFinished()`はアプリ側で実装する判定の例。待機アニメーションの周期末で終了演出を開始する、ユーザー入力を待つ、といった制御を自由に実装できる。

時間待ちと手動の終了指示を含むスクリプト例は`Docs/Examples/LoadingSceneController.as`。プロジェクトの`Assets/Application/Scripts/`へコピーしてScriptComponentから指定する。演出の具体的な再生と表示はアプリ側で追加する。

## Scene / SceneManager API

C++はSceneManager、AngelScriptは現在の`Scene@`から同じ操作を呼び出す。
これらのAPIはエンジンスレッドまたはそのスクリプト実行中に呼び出す。

| API | 意味 |
|---|---|
| `bool ChangeSceneWithLoading(target, loadingScene)` | ロード専用シーンへの遷移を予約し、その切り替え後に遷移先の準備を自動開始する。両方の登録が必要。同じ名前の指定は不可。 |
| `bool BeginSceneLoad(name)` | 現在のシーンを維持して、指定した登録済みシーンを準備する。ロード専用シーンへ通常の方法で移ってから呼び出してもよい。 |
| `bool IsSceneLoading() const` | ロード専用シーンへの遷移待ち、ファイル読み込み、構築中か。準備完了・失敗・キャンセル後はfalse。 |
| `bool IsSceneLoadReady() const` | 明示的に切り替え可能な状態か。完了しても自動的には移らない。 |
| `float GetSceneLoadProgress() const` | 0〜1。ファイル読み込み中は0、構築中は処理単位に応じて上昇、準備完了で1。バイト数の進捗や残り時間の推定ではない。 |
| `string GetSceneLoadTarget() const` | 準備中の登録名。完了待ちでも保持し、切り替え・キャンセル後にクリアする。 |
| `string GetSceneLoadError() const` | 失敗理由。失敗時は現在のシーンを維持する。再要求、キャンセル、通常の遷移でクリアする。 |
| `bool ActivateLoadedScene()` | 準備したシーンへの切り替えを予約する。実際の切り替えは描画後の通常のフレーム末尾。未完了や既に予約がある場合はfalse。 |
| `void CancelSceneLoad()` | 準備したシーンと切り替え予約を破棄する。実行中のファイルI/Oは結果を採用せず、現在のシーンを維持する。 |
| `bool SetSceneLoadBudgetMilliseconds(float)` | 1フレームに構築処理を続ける時間の目安。既定2ms。正の有限値で100ms以下。 |

同時に保持する有効な準備要求は1件。`BeginSceneLoad`の新しい有効な要求は以前の要求を置き換える。通常の`ChangeScene`、準備対象の登録解除、エディターのPlay Stopでも準備をキャンセルする。未登録名などで要求が拒否された場合、進行中の有効な要求は維持する。

## 処理の分離

- ファイル版のJSON読み込み・分割シーンの結合は別スレッドで実行する。ワーカーへ渡すのは確定した物理パスだけで、SceneやObjectのポインターは渡さない。
- Sceneの生成、シーンコンポーネント、オブジェクトの生成と設定はエンジンスレッドで段階的に進める。ロード専用シーンは毎フレーム通常どおり更新・描画する。
- 遷移先にはUpdate・物理シミュレーション・通常描画を実行しない。ScriptComponentはコンパイルまで行い、グローバル変数の初期化、Behaviorのコンストラクター、Awakeを切り替えまで保留する。Startは切り替え後の最初のUpdateで実行する。
- NormalWindowObjectとOverlayWindowObjectのネイティブウィンドウは切り替えまで生成しない。同名のウィンドウは従来の引き継ぎ機構を使う。
- ScreenBufferObjectは準備中に一時名のバッファを作り、ロード専用シーンの同名テクスチャへ干渉しないようにする。切り替え時に論理名へ反映し、既存バッファを引き継げる場合は引き継ぐ。
- シーン単位の入力設定は切り替え時に反映する。準備のキャンセルによって実行中シーンの入力設定をリセットしない。
- 読み込み失敗、不正なJSON、不正・重複UUID、未知のコンポーネント、スクリプトのコンパイル失敗ではReadyにせず、エラーを返す。通常の同期遷移の代替空シーンへのフォールバックとは異なる。
- ReleaseではEditorOnlyオブジェクトとその子孫を型検証・構築の前に除外する。Releaseに存在しない編集専用コンポーネントが保存されていても、読み込みエラーにしない。

準備中のネイティブコンポーネントのInitialize/LoadFromJsonは、リソース構築のために実行される。自作C++コンポーネントでゲーム開始時の副作用がある場合は、`IsPreparingScene()`または`SceneContext::IsPreparing()`で区別する。通常のフレーム更新で行う処理やスクリプトのAwake/Startは準備中には実行しない。

## 時間とメモリの注意点

構築の分割は協調的で、1つのオブジェクトやコンポーネントの初期化、1つのスクリプトモジュールのコンパイルを途中で中断するものではない。重い単位があるフレームでは設定した予算を超える可能性がある。大きな処理はオブジェクトやアプリ側の処理を分割する。Readyは演出終了や遷移先スクリプトの実行完了を意味しないため、ユーザー処理やウィンドウの最終設定にかかる切り替え時の処理時間も残る。

ロード専用シーンへの最初の切り替え自体は通常の同期遷移なので、そのシーンは軽量にしておく。準備中はロード専用シーンと遷移先の両方を保持するため、その分のメモリとGPUリソースが必要。

起動用の`LoadingAnimationSection`は起動スプラッシュを書き出すための設定で、通常のシーン内で区間を自動制御するものではない。通常のロード専用シーンでは、スクリプトから親オブジェクトの有効状態やKeyFrameAnimatorの再生を制御する。

## 検証

Developmentビルド後、VS x64 Developer PowerShellで`EngineTests/RunSceneLoadingRegression.ps1`を実行する。実際のSceneManager、分割形式のシーンファイル、AngelScriptを使い、ロード中のUpdate継続、手動切り替え、遅延するスクリプト初期化、シリアライズ値、失敗、キャンセル、ウィンドウ生成の保留、Play Stopを確認する。

### 2026-10-29の確認結果

- Development / ReleaseのMSBuild、配布生成、WorkCopy更新が成功。各構成のOutputs / WorkCopy / Distributionの実行ファイルのSHA-256一致を確認。
- Developmentの実SceneManager / AngelScriptを使うヘッドレス回帰テスト55項目が通過。準備中の更新、待機と明示的切り替え、初期化の実行タイミング、EditorOnlyの除外、キャンセル・失敗、終了コールバックからの次の要求、サンプルのコンパイル、終了時のグローバル変数保存を確認。
- テストではゲーム画面、GPU、音声デバイスの初期化は行っていない。実ゲームでのアニメーションの目視、フレーム時間、描画バッファとウィンドウの引き継ぎの目視確認は未実施。

ログは`Generated/SceneLoading/DevelopmentBuild.log`、`ReleaseBuild.log`、`Regression.log`。
