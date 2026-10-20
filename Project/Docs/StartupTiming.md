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
