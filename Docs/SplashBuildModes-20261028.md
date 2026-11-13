# スプラッシュの構成別分離（2026-10-28）

## 変更内容

| 構成 | 表示 | 終了時期・ウィンドウ |
|---|---|---|
| Development | 以前のWebView2＋起動ログ、640×360、枠なし・タスクバー非表示 | GameEngineの初期化終了後、最低表示800msとフェード200msを満たして閉じる。エディターのウィンドウとは独立 |
| Release | 現在のDirect2Dによるエンジン既定アニメーション | シーン構築後に終了演出を行い、同じHWNDを最初の通常ウィンドウへ引き継ぐ |
| Debug | Developmentと同じ実装をビルド対象に選択 | 構成評価のみ確認。今回の実起動確認はDevelopment/Release |

Releaseのアニメーション本体はビルドガードを加えただけで内容を維持。アプリ独自アニメーションをエディターで作成・書き出し・再生する機能は今後の拡張対象で、今回は既存の演出経路を構成別に分離した。

### 実装

- `Splash/SplashScreen.cpp`: Release専用。`RELEASE_BUILD`ガードとプロジェクトのExcludedFromBuildでDevelopment/Debugでは除外。
- `Splash/DevelopmentSplashScreen.cpp`: Git HEADに保存されていた以前のWebView2方式を復元。`!RELEASE_BUILD`限定で、Releaseのビルドから除外。
- HTML/CSS/JavaScriptは既存の`EngineData/UI/Splash`をそのまま使用し、起動ログの購読・100ms間隔の配信を復元。
- `KashipanEngine.cpp`: Development/DebugはGameEngineコンストラクターをスプラッシュのRAIIスコープで囲み、初期化完了時に閉じてからExecuteを呼ぶ。Releaseは従来どおり早期表示から初回シーンへの引き継ぎまでガードを維持。
- `GameEngine.cpp`: 終了アニメーション待ち、ウィンドウ引き継ぎ、描画後の表示切り替えをRelease限定にした。DevelopmentにはRelease用の待ちを加えない。
- Development側のHasStartupWindow/ClaimWindow等はfalse/null/no-op。既存Window実装が起動HWNDを予約したり、描画・設定反映を保留したりしない。
- Development/Debugの不要なD2D/DirectWriteの遅延ロード指定を除去。WebView2 SDKのinclude指定はDevelopment用ソースへ追加。
- VSプロジェクトとフィルターに新しいソースを登録。

### 旧実装からの終了処理の補強

外観・ログ配信・表示時間を維持しつつ、終了通知は専用UIスレッドへPostMessageで渡すようにした。WebView2のCOMインターフェースへメインスレッドから直接アクセスしない。最低表示時間とフェードはUIスレッドのタイマーで処理する。

Showの3秒待機がタイムアウトしても、ワーカーがシグナルするイベントはjoinまで保持。遅いWebView2コールバックは世代番号とHWND・終了要求を確認し、終了済み／次世代の画面へ適用しない。コントローラーのCloseとCOM解放もUIスレッドで行う。

HTMLやWebView2環境が利用できない場合もエンジン初期化は継続する。仮想ホストのローカルフォルダーマッピングに失敗した場合はNavigateせず、外部ホストへのフォールバックを避ける。

検証時だけ`KASHIPAN_SPLASH_USER_DATA_FOLDER`でWebView2プロファイルを隔離可能。未指定時は以前と同じLocalAppData/KashipanEngine/SplashWebViewを使用する。

## 検証結果

### 単体テスト

`EngineTests/RunDevelopmentSplashRegression.ps1`の4ケース合格。

- lifecycle: 640×360、WS_POPUP/WS_EX_TOOLWINDOW、独立UIスレッド、最低表示・フェード、繰り返し起動終了、Release用APIがウィンドウ予約を行わないこと。
- content: 実際のWebView2でロゴDOMと起動ログを確認。ログは末尾6行、HTMLに見える文字もtextContentとして表示。DOMアクセスがUI STAで行われることと、closing通知がページへ届くことも確認。
- user-close: 起動用ウィンドウをユーザーが閉じても、Releaseのキャンセル扱いにしない旧動作。
- missing-ui: UIがない場合も終了・次回起動で固まらないこと。

`EngineTests/RunStartupSplashRegression.ps1`をRelease明示で実行し、従来のタイムライン・描画・リソース復旧・ライフサイクル10ケースが合格。

### 実際のエディター起動

隔離したJobHuntingGameとUserSettingsで以下を確認。Error/Criticalなし。

- 旧クラス`KashipanEngineSplash`、640×360、枠なし・ツールウィンドウの表示。
- エディター側のHWND・所有スレッドはスプラッシュと別。Release用の`KashipanEngineStartupLogo`は出現しない。
- 初回描画までに旧スプラッシュのHWNDが破棄されていること。
- 保存済み最大化状態の復元。
- `Entry.GameEngineAndSplash`が記録され、Release用のclaim/handoff、ScenePrepared、SplashHandoffWaitは記録されないこと。

エディターの起動確認後、隔離した検証プロセスを終了。エディターの未保存確認を伴う通常終了や、手動の外観比較は今回の確認対象外。

### Releaseの実起動

同じ隔離プロジェクトで5ケースを確認。全てError/Criticalなし・終了コード0。

| ケース | 確認 |
|---|---|
| normal | 起動とゲーム表示で同じHWND/所有スレッド、表示通常ウィンドウ最大1つ、リサイズ、Alt+Enter全画面往復、通常終了 |
| minimized | シーン構築完了後に最小化を要求し、終了演出後も維持。復元・リサイズ・通常終了 |
| cancel | 起動直後の中断 |
| fade-close | 終了演出中の中断 |
| missing-images | Release用画像を隔離先で外しても同じHWNDで起動・終了 |

起動直後の最小化要求では、演出の前に復元された試行も1回観測した。原因や今回の構成分離との因果関係は未特定。`EarlyMinimizeObservation/Samples-minimized.json`に記録を保持し、実起動での最小化引き継ぎの合格範囲は上表の場面に限定する。Releaseアニメーション実装本体が変更前と一致することは別途確認した。

## ビルド・反映

Release/Developmentの変更対象をコンパイルし、既存オブジェクトとリンク。両構成の実起動を確認。全ソースのクリーンビルドではない。検証ビルドだけ既知のPDB環境問題を避ける`_CL_=/Z7`、`_LINK_=/DEBUG:NONE`を使用し、恒久的なPDB設定は変更しない。

MSBuildのgetItemでDebug/Development/Releaseを評価し、各構成でSplashScreen.cppとDevelopmentSplashScreen.cppのどちらか一方だけがビルド対象になることを確認した。

Release/Developmentの`Generated/Outputs/x64`と既存`Generated/WorkCopy/x64`へ実行ファイルを反映し、ハッシュ一致を確認。元のUserSettings、JobHuntingGame/Project.json、旧HTML/CSS/JavaScriptのハッシュは変更前と一致。

## 証跡

`Generated/SplashBuildModes-20261028/`へ保存。

- `DevelopmentRegression.log`、`ReleaseRegression.log`: 単体テスト。
- `DevelopmentRuntimeResults.json`、`DevelopmentRuntime.log`: エディター実起動。
- `RuntimeResults.json`、`Runtime-*.log`、`Samples-*.json`: Release実起動。
- `DebugProjectItems.json`、`DevelopmentProjectItems.json`、`ReleaseProjectItems.json`、`ProjectSelectionVerification.json`: MSBuild構成評価。
- `ReleaseBuild.log`、`DevelopmentBuild.log`、各構成のexeとリンク入力。
- `Verification.json`: 実行ファイル反映先・ハッシュ、ユーザー入力の不変確認。
- 再現用のPrepareBuild.py、PrepareQA.py、ReleaseRuntimeQA.py、DevelopmentRuntimeQA.py。

WebView2のDOM検証結果は`Generated/DevelopmentSplashRegression/Content.json`に保存。各試験のプロファイルも同フォルダー内へ隔離した。
