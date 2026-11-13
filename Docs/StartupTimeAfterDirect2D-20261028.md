# Direct2D移行後のエンジン起動時間（2026-10-28）

> 比較条件の補足: 本計測は元アセット付き・65モデルの通常キャッシュ構成です。以前の約2.34秒はCooked配布版・49モデルであり、直接比較できません。条件を揃えた調査は [起動時間増加の調査](StartupTimeRegressionInvestigation-20261028.md) を参照してください。

## 結果

現在のRelease x64、JobHuntingGameのTitleSceneを測定しました。
プロセス起動開始から、スプラッシュが終了してゲーム更新を再開できる状態になるまで、
エンジンキャッシュ再利用時の中央値は **6.19秒**（5回、6.16〜6.20秒）でした。

| 区間の終点（起点はいずれもプロセス起動開始） | キャッシュ再利用5回の中央値 | 最小〜最大 |
|---|---:|---:|
| スプラッシュが可視状態になる | 0.128秒 | 0.116〜0.164秒 |
| スプラッシュの最初の背景描画完了 | 0.134秒 | 0.121〜0.169秒 |
| 最初のシーンの描画完了通知 | 4.819秒 | 4.568〜5.051秒 |
| スプラッシュ終了・UIスレッドの後始末完了 | 6.186秒 | 6.160〜6.203秒 |

読み込み完了後、現在の1回転を終えて1秒のフェードを行うため、
描画完了通知からスプラッシュ終了までの待ちは中央値 **1.37秒** でした。
そのため、読み込みが少し短縮されても同じ回転の終了境界に収まる場合、全体の終了時刻はほぼ同じになります。
UIスレッドの終了後、メインスレッドが待機ループから復帰するまで最大約8msのポーリング待ちが加わり得ます。

エンジンキャッシュを持たない隔離先での初回は、最初のシーン描画まで **15.75秒**、
スプラッシュ終了まで **17.51秒** でした。
これはエンジンキャッシュ生成を含む1回の参考値です。OS・GPUドライバーのキャッシュは消しておらず、
PC再起動後の完全なコールド起動を測ったものではありません。

## 全試行

| 試行 | 条件 | スプラッシュ可視化 | 初期シーン描画完了通知 | スプラッシュ終了 | 描画完了後の演出・後始末待ち | Error/Critical |
|---|---|---:|---:|---:|---:|---:|
| 0 | エンジンキャッシュ未作成 | 0.455秒 | 15.750秒 | 17.508秒 | 1.758秒 | 0 |
| 1 | キャッシュ再利用 | 0.164秒 | 5.051秒 | 6.203秒 | 1.152秒 | 0 |
| 2 | キャッシュ再利用 | 0.129秒 | 4.917秒 | 6.188秒 | 1.271秒 | 0 |
| 3 | キャッシュ再利用 | 0.128秒 | 4.819秒 | 6.186秒 | 1.366秒 | 0 |
| 4 | キャッシュ再利用 | 0.117秒 | 4.568秒 | 6.167秒 | 1.599秒 | 0 |
| 5 | キャッシュ再利用 | 0.116秒 | 4.597秒 | 6.160秒 | 1.563秒 | 0 |

## 従来のStartupTimingログとの区別

以下はエンジン処理の入口からの計測であり、上表の「プロセス起動開始から」と起点が異なります。

| ログ | キャッシュ再利用5回の中央値 |
|---|---:|
| `Startup.InitializationTotal` | 4.292秒 |
| `Startup.Total (engine entry -> startup scene first frame drawn)` | 4.747秒 |

## 読み込み中の主な区間

キャッシュ再利用時のログの中央値です。以下は主要区間の抜粋であり、全体の単純な内訳合計にはしません。
親区間に子区間を含む場合があり、中央値同士の和も各試行の合計とは一致しません。

| 区間 | 中央値 |
|---|---:|
| `Engine.GraphicsAndPipelines` | 1.460秒 |
| `Engine.Models` | 0.790秒 |
| `Engine.DirectX` | 0.753秒 |
| `Engine.Audio` | 0.534秒 |
| `Engine.Textures` | 0.310秒 |
| `Engine.Input` | 0.138秒 |

## 条件・方法・範囲

- 現行Releaseのオブジェクトを使い、スプラッシュのオブジェクトだけに時刻記録を加えた計測用exeを別に作成。
  描画方式・演出時間・シーン読み込み方式は変更していません。
- JobHuntingGameのAssetsとエンジンのロケール・スプラッシュ画像を新しい隔離先へコピー。
  初回前に隔離先のCacheが存在しないことを確認し、以後5回は同じ配置と生成済みキャッシュを再利用。
- 各キャッシュ再利用試行でシェーダー119件、PSO108件、テクスチャ47件、モデル65件のキャッシュヒットを確認。
  モデル再解析は0件。全6回でError/Criticalは0件でした。
- 全試行でDirect2Dのハードウェア描画を確認。
- 親プロセスがCreateProcessを開始する直前にQueryPerformanceCounterを記録。
  子プロセスも同じカウンターで、背景描画後・NotifyReady呼び出し・WM_DESTROY・UIスレッド終了を記録。
- 「描画完了通知」は最初のシーンの描画処理が復帰してStartup.Totalを記録した直後のNotifyReadyです。
  画面走査・DWM表示完了を測っているものではありません。
- 「スプラッシュ終了」は、ウィンドウを閉じてDirect2D/COM等の後始末を終え、
  IsActiveをfalseへ切り替える直前。ゲーム操作の体感テストは行っていません。
- 可視状態の検出だけは対象PID・専用ウィンドウクラスを5ms間隔でポーリングするため、
  ポーリングとスケジューリングの遅延を含みます。
- CPU/GPUクロック、他アプリの負荷、OSファイルキャッシュは固定していません。
  今回のPC・配置・プロジェクトでの測定値です。
- 時刻データ保存の終了を確認した後、隔離した計測用プロセスだけを終了。
  プロセスの通常終了を検証する試験ではありません。
- 元のソース4ファイル、UserSettings、Project.json、通常Release exeのSHA-256が計測前後で一致。
  計測用ビルドのみPDB環境エラー回避設定を使用し、通常のビルド設定は変更していません。

## 生データと再現用ファイル

- [StartupTimes.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/StartupTimes.csv)
- [Results.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/Results.json)
- [Summary.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/Summary.json)
- [Inputs.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/Inputs.json)
- [InputVerification.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/InputVerification.json)
- [Measure.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/Measure.py)
- [Analyze.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/Analyze.py)
- [BuildProbe.cmd](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/BuildProbe.cmd)
- [SplashScreenInstrumented.cpp](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupTimeD2D-20261028/SplashScreenInstrumented.cpp)

各試行の起動ログ、フレームCSV、イベント時刻JSONも同じフォルダーに保存しています。
再実行時は、新しい隔離先を使う必要があります。既存キャッシュや結果の誤用を防ぐチェックを入れています。
