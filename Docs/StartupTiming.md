# 起動時間のログ

起動時に `[StartupTiming] 区間名: 123.456 ms (0.123 s)` の形式で、既存のロガーへInfoレベルで出力します。ログ設定でエンジンのInfo出力とファイル出力を有効にしてください。

- `Entry.*`: 翻訳、プロジェクト決定、設定、ロガー、スレッドプール、エンジン生成とスプラッシュ終了。
- `Engine.*`: DirectX、各アセットManager、入力、シーン一覧、エディターUI、AppInitializeなど。
- `Pipelines.*`: シェーダー・状態プリセットの読み込みとPSO生成。
- `Textures.*`: ファイル探索、代替テクスチャ、デコード・ミップ生成、GPU転送・登録。
- `Models.*`: ファイル探索、Assimp解析、登録・Prefab生成。
- `Audio.*`: ファイル探索、デコード、登録。
- `FirstFrames.*`: 起動中の更新、描画・Present、シーン切り替え・後処理。
- `Startup.InitializationTotal`: エンジン入口からGameEngine生成とスプラッシュ終了までの累計。
- `Startup.Total`: エンジン入口から初期シーンを構築し、そのシーンの最初の描画処理が完了するまでの累計。
- `Startup.Interrupted`: 初期シーンの描画完了前に終了・例外になった場合の経過時間。

計測には単調増加時計（steady_clock）を使い、I/O、並列ジョブの完了待ち、GPU待機も含む実経過時間を記録します。ロガー初期化前の区間は一時保存し、初期化後に出力します。計測開始はKashipanEngine::Executeの入口であり、OSのプロセス生成やその前のDLLロードは含みません。描画完了はGameLoopDrawの復帰時点であり、画面が実際にディスプレイに表示された時刻の計測ではありません。

Releaseはスプラッシュの生成・表示・終了処理を実行せず、エンジン生成区間を`Entry.GameEngine`として記録します。Debug／Developmentはスプラッシュを表示し、従来どおり`Entry.GameEngineAndSplash`へその生成・終了待ちも含めます。どちらも`Startup.InitializationTotal`と`Startup.Total`の計測範囲は同じです。

親区間は子区間の時間を含みます（例: Engine.TexturesにはTextures.*が含まれます）。階層をまたいで足し合わせないでください。起動完了後はこれらの計測ログを出力しません。

追加の内訳計測とキャッシュ・転送のログ、事前変換ツールについては[StartupOptimization.md](StartupOptimization.md)を参照してください。

## 起動時のアセット探索

Assetsの共通一覧はCOM初期化後に別スレッドで一度だけ作成し、Windows・DirectX・描画パイプラインの初期化と並行して進めます。テクスチャマネージャーの生成直前に完了を待ち、以降は従来どおり各マネージャーが一覧を使用します。ワーカーはコピーした物理ルートとローカルの一覧だけを扱い、ログ出力・マネージャー登録・GPU操作は行いません。探索で例外が発生した場合は結果取得時にメインスレッドへ伝播します。

| ログ名 | 計測対象 |
|---|---|
| `Engine.AssetScanDispatch` | ルートの取得と非同期探索の開始処理 |
| `Engine.AssetScan` | ワーカー内で一覧を探索・分類する実経過時間。結果取得後にメインスレッドから出力 |
| `Engine.AssetScanWait` | 描画初期化直後の結果取得までの区間。完了済みなら受け渡しだけで済む |
| `Engine.AssetScanReporting` | 探索時間とファイル・ディレクトリー数のログ出力 |

`Engine.AssetScan`はほかの初期化区間と時間が重なるため、起動時間へ加算しないでください。非同期化前の同名区間は同期探索と件数ログを含んでおり、計測範囲がわずかに異なります。探索自体が速くなる変更ではなく、ほかの初期化との重なりで起動時の待ち時間を減らす変更です。


## シーン切り替えの大区間

実際にシーンを切り替えたとき、`[SceneChangeTiming]` をInfoレベルで出力します。起動時と起動完了後の切り替えの両方が対象です。切り替え要求がない通常フレームには出力しません。

| ログ名 | 計測対象 |
|---|---|
| `Manager.BeginCarryOver` | 描画先リソースの引き継ぎ受付の開始 |
| `Manager.FinalizePreviousScene` | 旧シーンの終了処理 |
| `Manager.DestroyPreviousScene` | 旧シーンの破棄。デストラクター内のバックアップ保存・オブジェクト等の破棄も含む |
| `Manager.ReadSceneJSON` | 分割／単一JSONの読み込み。メモリ上のJSON登録や空シーンではファイル読み込みなし |
| `Manager.ConstructScene` | SceneのコンストラクターとLoadFromJSON。オブジェクト・コンポーネントの生成、および生成中に行う初期化も含む |
| `Manager.InitializeScene` | SceneManagerの設定、再生状態の引き継ぎ、InitializeInterfaceの実行 |
| `Manager.FinishCarryOverAndSnapshot` | 旧スナップショットの解放と、引き取られなかった描画先リソースの破棄 |
| `Manager.Total` | SceneManager内のシーン切り替え全体。対象シーン名も併記 |
| `Frame.CommitSceneChange` | SceneManagerによる切り替え処理全体。上記Manager各区間を含む |
| `Frame.RendererCleanup` | 切り替え後のレンダラーリソース解放、デルタタイム等のリセット |
| `Frame.PendingResourceDestruction` | 保留中のスワップチェーン・ウィンドウ・描画バッファ・動画/GIF等の破棄 |
| `Frame.Total` | フレーム末尾で行うシーン切り替えと後処理の全体 |

例: `[SceneChangeTiming] Manager.ConstructScene: 2345.678 ms (2.346 s) (scene=Stage1)`。

`ConstructScene`内ですでにコンポーネントの初期化が呼ばれるため、初期化コストのすべてが`InitializeScene`へ入るわけではありません。最初は上記の大区間で確認し、重い区間だけを追加で分割します。

計測にはsteady_clockを使います。親区間は子区間を含むため、`Manager.Total`や`Frame.CommitSceneChange`とManager各区間を重ねて加算しないでください。各区間のログ呼び出し時間は次の区間から除きますが、Totalにはログ出力のオーバーヘッドも含みます。最初の更新・描画やディスプレイへの表示完了はこの計測に含みません。`Frame.Total`のログを書き込んだ時点でログファイルをフラッシュします。


## シーン構築の詳細計測

SceneManagerによる切り替え中は、内側の処理を`[SceneLoadTiming]`で集計します。切り替え終了時に、ラベルごとの合計時間・回数・1回の最大時間・最大時間だった対象を出力します。通常フレームやバックグラウンドスレッドの処理はこの集計に含みません。

- `SceneConstructor.*`: SceneContext、EditorContext、エディター生成とコンストラクター全体。
- `Editor.*`: SceneView、AssetsWindow、外部変更監視の開始、同期的なPrefabファイル列挙とPrefabキャッシュ読み込み。
- `AssetsWindow.*`: フォルダーツリー全体、各フォルダーの再帰処理、論理パス変換、ルートのファイル一覧。
- `PrefabIndex.*`: 初回のPrefab索引作成に伴う全ファイル列挙とPrefab JSONの読み込み・登録。すでに索引がある場合はこの区間は出ません。
- `SceneJSON.*`: シーンコンポーネント生成・初期化、JSON反映、オブジェクト生成、オブジェクトJSON反映、変数復元。
- `ObjectJSON.*`: オブジェクトごとの復元と既存コンポーネント破棄の合計。
- `SceneComponent.*.<型名>` / `ObjectComponent.*.<型名>`: コンポーネントの一時インスタンス生成、追加と初期化、JSON反映を型ごとに集計。
- `ScriptEngine.*` / `Script.*`: AngelScriptエンジン生成、バインディング登録、補完用定義生成、スクリプトの再読み込み・モジュールビルド。実際に呼ばれた処理だけが出力されます。

例: `[SceneLoadTiming] ObjectComponent.LoadJSON.Transform: total=1.234 ms, count=141, max=0.123 ms (scene=Stage1) (slowest=Player)`。

すべて実経過時間で、合計値は包含時間です。たとえば`SceneConstructor.Total`は`Editor.Constructor.Total`を含み、`AssetsWindow.BuildFolderNode.Inclusive`の合計は再帰の親と子を重ねて数えるため、実際のツリー構築時間より大きくなります。ツリー全体の時間は`AssetsWindow.BuildFolderTree`で確認してください。異なる階層の値を足し合わせないでください。

詳細計測自体の時計取得・集計コストが加わるため、計測なしの以前の値との厳密な速度比較には使用しません。最大時間の対象は、そのシーン切り替え中に最も長かったオブジェクト名・パス等です。バックグラウンド監視は開始処理のみ計測し、別スレッドの走査完了時間は含めません。

## エディターのアセット階層の再利用

フォルダーツリーはプロセス内で直近の1プロジェクト分を保持し、プロジェクト・Assetsルート・論理ルートが一致する場合、シーン切り替え後のAssetsWindowでも再利用します。選択・検索・エディターの状態はウィンドウごとに保持し、ルートの翻訳ラベルは描画時に取得します。ディスクへのキャッシュ保存は行いません。

新規構築では論理ルートを一度変換し、子フォルダーの論理パスを親パスと名前から組み立てます。各フォルダーでの`filesystem::relative`によるパス解決を省きます。

| ログ名 | 計測対象 |
|---|---|
| `AssetsWindow.AcquireFolderTree` | 新規構築または共有済みツリーの取得 |
| `AssetsWindow.BuildFolderTree` | 実際にフォルダーツリーを構築した場合の時間 |
| `AssetsWindow.ReuseFolderTree` | 共有済みツリーを取得した場合の時間。シーン切り替え全体やファイル一覧の更新は含まない |
| `AssetsWindow.ResolveLogicalPath` | 親パスと子の名前からの論理パス組み立て |

手動更新やエディター内のフォルダー操作では再構築します。外部変更は既存のバックグラウンド走査の完了結果と照合し、空フォルダーを含む構成の変更時だけ再構築します。ファイル内容だけの変更ではファイル一覧を更新し、ツリーは再利用します。走査が不完全な場合はその結果による照合を行いません。シーン切り替えを待たせないため、外部で変更された階層は走査完了まで一時的に古い状態で表示されることがあります。

`EngineTests/RunAssetFolderTreeRegression.ps1`は、ウィンドウ破棄後の再利用、プロジェクトの分離、構成変更の検出を検証します。`-BenchmarkProject`で指定したプロジェクトのAssetsに対して旧方式の探索・新規構築・再利用の時間も比較できます。この比較はツリー処理単体であり、実際のGUI操作やシーン切り替え全体の計測ではありません。
