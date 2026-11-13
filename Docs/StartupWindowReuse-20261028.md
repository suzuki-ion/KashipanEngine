# 起動ウィンドウの引き継ぎ（2026-10-28）

## 動作

起動時に作成した通常のWin32ウィンドウを破棄せず、最初に作成する通常ウィンドウへ引き継ぐ。Releaseではゲームの最初の通常ウィンドウ、Developmentでは先に作成されるエディターの「ImGui Window」が対象。同じHWND・所有スレッドを維持する。

1. エンジンのメインスレッドでウィンドウを作り、背景を先に表示。
2. Direct2D/DirectWrite/WICの生成・アニメーション描画だけを専用ワーカーで実行。ワーカーにはHWNDと画像フォルダーを渡し、WindowやSceneのポインターを渡さない。
3. 初回シーンの構築中、最初の通常ウィンドウは既存HWNDを予約。タイトル・サイズ・アイコン等の変更とDX12スワップチェーン作成を保留。他の起動中ウィンドウも非表示で準備する。
4. シーン構築完了を通知し、現在の回転終了とフェードを待つ。ゲーム更新は進めず、ネイティブメッセージは処理する。
5. アニメーションのワーカーをjoinし、D2D/WIC/COMリソースの解放を確認。同じHWNDのウィンドウプロシージャをエンジン用へ切り替え、アプリの設定を反映してDX12スワップチェーンを作成。
6. 初回シーンを描画してから、保留していた追加ウィンドウを表示。初回GPU準備を含む待ち時間をゲームのdelta timeへ持ち込まないようリセットする。

ウィンドウは別スレッドへ移管しない。最初から最後までメインスレッドが所有し、メッセージ・Windowマップ・イベントハンドラーも従来どおりメインスレッドで扱う。

## 設定・終了時の扱い

- タイトル、アイコン、サイズ、位置、サイズ変更モード、全画面、表示状態、親ウィンドウ、エディター保存済みWINDOWPLACEMENTは引き継ぎまで保留。
- 起動中にユーザーが最小化した場合は、引き継ぎ後も最小化を維持。
- エディター保存済みの最大化・復元位置は引き継ぎ時に適用。起動用のサイズをUserSettingsへ上書きしない。
- 画像やD2Dの初期化に失敗しても、作成済みHWNDをそのままゲームへ渡す。
- 起動中のWM_CLOSEはキャンセルと描画停止を要求。描画ワーカーを停止・解放してからHWNDを破棄し、描画先の破棄競合を防ぐ。
- 並列アセット処理の待機中はメッセージだけを処理し、キャンセル例外はジョブ完了後の安全な境界で送出する。キューが参照するローカルデータを途中で破棄しない。
- シェーダー・パイプラインの列挙中にも起動メッセージとキャンセルを確認。
- エディターのImGuiフレーム・プロファイラーUI・プラットフォームウィンドウの更新開始も引き継ぎ後へ移した。起動中だけImGuiのフレーム完了処理を省略する方式は使わない。
- 引き継ぎ対象は通常ウィンドウ。オーバーレイしか作成しない構成では、演出終了後に未使用の起動HWNDを破棄し、オーバーレイを表示する。
- アプリが複数ウィンドウを定義している場合やエディターのゲーム用ウィンドウは、演出終了後には通常どおり表示される。起動用とメイン用の重複を解消する変更であり、アプリの複数ウィンドウ機能は維持する。

## 計測ログの区別

表示先の競合を防ぐため、終了演出の開始条件を「初回描画完了」から「初回シーンの構築完了」に変更した。

| ログ | 範囲 |
|---|---|
| `Startup.ScenePrepared (engine entry -> scene ready for presentation)` | エンジン入口から初回シーンの構築完了まで。演出終了待ち・起動用スワップチェーン作成・最初の描画準備を含まない |
| `FirstFrames.SplashHandoffWait` | 終了演出待ちとウィンドウ設定反映、DX12スワップチェーン作成 |
| `Startup.Total (engine entry -> startup scene first frame drawn)` | エンジン入口から初回シーン描画まで。今回から演出待ちも含む |

以前のStartup.Totalは演出待ちの前に記録していたため、以前の「純粋な読み込み時間」とそのまま比較しない。ScenePreparedも最初のGPU描画準備を含まないため、以前のTotalと完全に同じ範囲ではない。プロセス生成・DLLロード・DWMの走査完了は従来どおり対象外。

## 検証

`EngineTests/RunStartupSplashRegression.ps1`で次を確認。

- タイムライン、座標系、画面サイズ対応。
- WICターゲットの描画ピクセル、実HWNDへのD2D生成・破棄・再生成・リサイズ・ソフトウェアフォールバック。
- early / late / cancel / missing / user-close / minimized。
- handoff / handoff-minimized / handoff-missing: 同じHWND・所有スレッドを保持し、プロシージャ切り替え後も有効。古いスプラッシュのCloseでゲームウィンドウを破棄しない。
- abandon: 予約したHWNDを起動途中の失敗経路で安全に破棄。重複Closeと自然終了で誤ってキャンセル扱いにならない。

### Releaseの実起動

隔離したJobHuntingGame / TitleSceneを実行し、対象PIDの表示ウィンドウを5ms間隔で監視。次の5ケースは全てError/Criticalなし、終了コード0。

| ケース | 確認内容 | 表示中の通常ウィンドウ最大数 |
|---|---|---:|
| normal | 同じHWND/所有スレッド、リサイズ、Alt+Enterで全画面往復、ゲーム側WM_CLOSEで通常終了 | 1 |
| minimized | 最小化状態で引き継ぎ、復元・リサイズ・通常終了 | 1 |
| cancel | 起動直後の終了要求で中断 | 1 |
| fade-close | シーン構築完了後、終了演出中の終了要求で中断 | 1 |
| missing-images | 隔離先のスプラッシュ画像を外し、同じHWNDでゲームを起動・終了 | 1 |

最初に観測したHWND/所有スレッドと、`[StartupWindow] claimed`・`handoff`のログを照合。起動時とゲーム表示後で同一であることを確認した。ポーリングであり、OSイベント全ての完全な追跡ではないが、別のメインウィンドウを作らないことは実装経路でも確認。

### Developmentの実起動

エディターの初回描画まで実行し、起動HWNDが「ImGui Window」へ引き継がれることを確認。引き継ぎ前の表示通常ウィンドウは最大1つ。隔離UserSettingsに設定した最大化状態の復元も確認。引き継ぎ後はシーンが定義するゲーム用ウィンドウも通常どおり表示された。Error/Criticalなし。

起動中にHWNDを予約した後のキャンセルも、表示最大1ウィンドウ・終了コード0で確認した。

エディターの起動後の終了要求は既存の未保存確認に入ったため、通常終了の合格とはしていない。起動・最大化確認後、隔離した検証用プロセスを終了した。Releaseではゲーム側の通常終了まで確認済み。手動の外観比較、別GPU/別PC、長時間操作、オーバーレイ専用構成は未検証。

## ビルド・反映・変更範囲

- ReleaseとDevelopmentの変更対象をコンパイルし、既存オブジェクトとリンク。両方の実起動を確認。全ソースのクリーンビルドではない。
- 検証ビルドのみ既知のPDB環境問題を避ける`_CL_=/Z7`、`_LINK_=/DEBUG:NONE`を使用。恒久的なプロジェクト設定は変更しない。
- Release・Developmentそれぞれの`Generated/Outputs/x64`と既存`Generated/WorkCopy/x64`へexeを反映し、SHA-256一致を確認。
- 元のUserSettings/Settings.jsonとJobHuntingGame/Project.jsonのSHA-256が計測前の値と一致。画像を外す試験は隔離先のみで行い、終了後に復元。
- 製品コード: SplashScreen、Window、GameEngine、エントリーの非同期待機処理、PipelineManagerのメッセージ処理、ImGuiManagerの保存済み配置復元・保存ガード。
- 起動アニメーション自体のキーや座標・演出時間、シーン間のウィンドウ引き継ぎ、外部ImGuiライブラリは変更していない。

## 証跡

`Generated/StartupWindowReuse-20261028/`へ保存。

- `RuntimeResults.json`、`Runtime-*.log`、`Samples-*.json`: Release 5ケース。
- `DevelopmentRuntimeResults.json`、`DevelopmentRuntime.log`、`DevelopmentCancelResults.json`、`DevelopmentCancel.log`: エディター起動・取消。
- `SplashRegression.log`: 単体テスト結果。
- `RuntimeQA.py`、`DevelopmentQA.py`、`DevelopmentCancelQA.py`: 再現用スクリプト（隔離先前提）。
- `ReleaseV3Build.log` / `ReleaseV3.exe`、`EditorV4Build.log` / `Development/EditorV4.exe`: 最終ビルド。
- `Verification.json`: 実行ファイルの反映先・ハッシュとユーザー入力の不変確認。
- `Before/`: 今回の主要変更対象の変更前コピー。過去のユーザー変更を含む作業状態を保存したもので、Git HEADとの差分ではない。
