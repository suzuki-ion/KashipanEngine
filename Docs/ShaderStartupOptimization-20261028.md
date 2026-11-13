# シェーダー起動準備の短縮（2026-10-28）

## 実装

優先候補1〜3を実装。PSOの遅延生成・並列化は含めません。

- `MaterialLayout::BuildCachedFromHlslSource`: レイアウト・注釈・Texture2D/Cube名を`Cache/Startup/MaterialLayouts`へ保存。既存ContentCacheコンテナーのSHA-256検証・不可分書き込み・保存失敗時フォールバックを利用。
- 依存元はDXCの有効なincludeだけでなく、レイアウト解析が実際に読んだすべてのテキストincludeを追跡。欠落したincludeも記録し、後からの作成を検出。ソース・includeの内容変更、マクロ名集合、絶対パス、パーサーバージョンで無効化。マクロ値は既存レイアウト解析が参照しないため、レイアウトキーは名前集合。バイトコード側は従来どおり値もキーへ含む。
- 空レイアウトも正常な解析結果として保存。破損したコンテナー・不正なレイアウトJSONでは再解析して修復。Cookedの生成・復元経路はそのまま利用。
- 同じ内容のバイトコードはコンパイラー内で共有し、リフレクションも内容ハッシュで共有。別名のシェーダーには独立したID・所有ハンドルを作り、破棄や再読み込みのロールバックに影響させない。再読み込み開始時に共有表をクリアして世代間の蓄積を制限。
- 通常シェーダーキャッシュの確認を先に行い、ソースblob・DXC include handlerの生成はミス時だけ行う。
- キャッシュ保存先の書き込み可否確認はコンパイラー内で1回に集約。
- 同じCompileShader呼び出し内では、バイトコード／MaterialLayoutに共通する依存ファイルの内容ハッシュを共有。ContentCache::DependencyValidationは明示的に渡した場合だけ有効で、呼び出しごとに破棄。要求をまたぐ更新時刻ベースの省略は行わず、同サイズ・同更新時刻の変更も検出する。
- `[ShaderCacheTiming]`でバイトコード準備・レイアウト処理・リフレクションの累積時間、`[StartupCache] Shader reuse`で共有回数をログ出力。

## 同条件での実測

Release x64 / JobHuntingGame / TitleScene / 通常キャッシュ65モデル / スプラッシュあり。双方のウォームアップ1回後、改修前後を交互に各5回起動。起点はエンジン処理入口。プロセス生成・DLLロード・DWM表示完了は含みません。

| 区間 | 改修前 | 改修後 | 短縮 |
|---|---:|---:|---:|
| `Pipelines.ShaderAndStatePresets` | 0.470秒 | 0.166秒 | +0.304秒 |
| `Pipelines.PSOCreation` | 0.278秒 | 0.282秒 | -0.005秒 |
| `Engine.GraphicsAndPipelines` | 0.776秒 | 0.477秒 | +0.299秒 |
| `Startup.InitializationTotal` | 2.659秒 | 2.336秒 | +0.323秒 |
| `Startup.Total (engine entry -> startup scene first frame drawn)` | 2.964秒 | 2.612秒 | +0.353秒 |

シェーダー・設定準備は約64.6%、パイプライン準備全体は約38.5%短縮。PSO作成・復元区間自体の短縮は確認していません。

- Before: 初回描画まで 2.900〜3.056秒。
- After: 初回描画まで 2.595〜2.759秒。

改修後の測定5回はすべてShader119ヒット／ミス0、PSO108ヒット／ミス0、モデル65ヒット／再解析0、画像47ヒット。MaterialLayoutは毎回119ヒット／再解析0、キャッシュファイル80件。バイトコードとリフレクションは毎回39件の共有。全12起動でError/Criticalなし。レイアウトキャッシュの内容ハッシュも測定中は不変でした。

改修後の読み込み終了後待ちは中央値約1.468秒、改修前は約1.112秒。今回の読み込み完了は3秒の導入演出より早いため、スプラッシュは導入3秒＋フェード1秒の演出時間を待ちます。読み込み短縮がそのままスプラッシュ表示終了の短縮にはなりません。演出時間・回転終了条件は変更していません。

CPU/GPUクロック、他プロセス、OS/ドライバーのキャッシュは固定していません。以前の4.62秒との直接比較ではなく、今回同じデータと配置で測った2.96秒→2.61秒が改修前後の比較です。

## 検証

- ShaderCacheRegression: include変更、同じサイズ・更新時刻の変更、マクロ、破損、無効HLSL、bytecode共有、reflection共有、独立ハンドル、再読み込みの成功／ロールバック時の世代破棄。
- MaterialLayoutの追加ケース: 注釈だけの変更、Texture2D/Cube復元、マクロ差、欠落include作成、include削除／復元、破損コンテナー、不正スキーマ、保存不能時の解析継続。
- CookedAssetsRegression: 移動、日本語パス、元ソースなしの復元、MaterialLayout復元、失敗経路。
- PipelineStateCacheRegression: Graphics/Compute再利用、shader/state/root変更、破損、ドライバー拒否。
- 計測なしの通常Release版でも初回描画＋その後2秒にError/Criticalなしを確認し、`Generated/Outputs/x64/Release/KashipanEngine.exe`へ反映。
- Release: 変更したShaderCompiler・MaterialLayout・PipelineManager・CookedAssetsをコンパイルし、既存オブジェクトとリンクして実際に上記12回起動。失われていた既存GameEngineオブジェクトも同じソースから再コンパイル。
- Development: 変更した上記4翻訳単位をUSE_IMGUI有効でコンパイル。エディター画面からの再読み込み操作やDevelopment全体リンクは未確認。
- 通常MSBuildでは外部ライブラリ・広範囲の再コンパイルが始まったため中断し、対象コンパイルとリンクへ切り替え。全ソースのクリーンビルドは未確認。
- 検証用ビルドのみ既知のPDB環境問題を避ける`_CL_=/Z7`、`_LINK_=/DEBUG:NONE`を使用。プロジェクトの恒久設定は変更しない。
- スプラッシュ本体・GameEngine・エントリーソース・Project.json・UserSettingsは前回のハッシュと一致。アセット変換や元アセットの変更は行わない。
- 子プロセスは初期化データ保存後に終了。通常終了や長時間描画の確認ではない。

## 証跡

`Generated/ShaderStartupOptimization-20261028/`に以下を保存。

- `FinalMeasurements/Results.json`、`Summary.json`、各試行のログ・CSV・イベントJSON。
- `Before.exe`、`After.exe`（同じスプラッシュ計測コードを使用）、`MeasureFinal.py`。
- `PrepareBuild.py`、`BuildTargeted.cmd`、`BuildMissing.cmd`、`AfterLink.rsp`、`ProductFinalLink.rsp`。
- `ShaderRegression.log`、`CookedRegression.log`、`PSORegression.log`、`DevelopmentCompile.log`、`ProductFinalLink.log`、`ProductSmoke.log`、`ProductVerification.json`。

最初の実装では永続キャッシュを追加しただけで、シェーダー準備が0.499秒→0.788秒へ悪化しました。保存先の確認を毎回繰り返さないこと、依存検証の重複を抑えることを追加してから上記の改善を確認しています。初回比較は`FirstAttemptResults.json`、`FirstAttemptSummary.json`とルート直下の試行ログに保持しています。
