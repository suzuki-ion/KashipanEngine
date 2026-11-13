# Release配布の固定参照解析

`EngineTools/AnalyzeReleaseAssets.py` は配布用ステージングを解析し、未参照と判断できたアセットだけを除外する。`CreateDistribution.ps1` がキャッシュ生成前に `--apply` 付きで呼び出すため、除外した元アセットのキャッシュも生成しない。`GameProjects/*/Assets` は変更しない。

## 今回の対象範囲

- すべてのシーン、スクリプト、一般データ、エンジン所有アセットを参照元として保持する。シーンの登録経路や到達不能コードの解析、未使用スクリプトの削除は行わない。
- JSON / JSONCの文字列値、スクリプトの文字列リテラル、相対includeから固定参照を追跡する。コメントを参照として扱わない。
- パス・ファイル名・拡張子なしの名前に加え、マテリアルJSONの `name` を解決する。同名の候補が複数ある場合はすべて保持する。
- マテリアルから画像などへの参照も追跡する。未参照のマテリアルと対応メディアを除外できる。
- 対象メディアは画像・モデル（OBJを含む）・音声・動画・フォントとモデル補助ファイル。未知形式は除外しない。汎用バイナリの `.bin` は用途を確定できないため単独でも保持する。

## 判断できない場合

既知のアセット取得・設定APIの引数が変数や式なら、その種類の全アセットを保持する。`SetMaterialNameAt` の第2引数、文字列連結によるパス、解決できない固定名、解析できないJSONについても保持理由を記録する。定数伝播・関数の戻り値・文字列生成結果は推測しない。

モデル内部のテクスチャ・マテリアル名・外部バッファは今回の解析対象外なので、保持するモデルがある場合は画像・マテリアル・モデル補助ファイルを保守的に保持する。エンジンが暗黙に使うフォント・標準画像等も引き続き保持する。

これは任意のユーザー定義APIやネイティブC++の動作まで証明する解析ではない。新しい動的アセット取得APIを追加した場合は、解析側の `SCRIPT_CALLS` も対応させる。

## Prefab

ReleaseでPrefabを利用しない方針により、`.prefab` は参照の有無やエンジン所有かどうかに関係なく除外する。ディレクトリ形式のPrefabも対象。Prefab内を解析しないため、Prefabだけが参照しているアセットを保持する理由にはしない。ただし、他の動的参照により同じ種類全体を保持する場合は残る。

Development配布ではこの解析処理を実行しないため、Prefabは引き続き含まれる。

## レポートと確認方法

```powershell
# 解析のみ。元データに --apply は指定しない。
python EngineTools/AnalyzeReleaseAssets.py --assets-root GameProjects/JobHuntingGame/Assets --report Generated/AssetUsage.json

# 固定参照・保守的な保持・Prefab除外の回帰テスト
python EngineTests/ReleaseAssetAnalysisRegression.py
powershell -File EngineTests/RunDistributionLayoutRegression.ps1
```

実配布のレポートは `Generated/Distribution/x64/Release.asset-usage.json` に保存する。

| 項目 | 内容 |
|---|---|
| `referenceEdges` | 参照元・参照先・JSON位置または行番号・固定文字列 |
| `retentionReasons` | ファイルごとの保持理由 |
| `uncertainDependencies` | 動的参照等の発生元と種類・理由・行番号 |
| `excludedDetails` | 除外するファイルの理由・種類・容量 |
| `diagnostics` | 文字コードやJSON形式を解析できなかった箇所 |

JobHuntingGameでの今回の解析では、Prefab64件と未参照音声 `Application/Sound/seUISelect.mp3` 1件、合計1,496,185 bytes（約1.43 MiB）が除外対象になった。モデル・画像・マテリアルには動的参照や未解析のモデル依存があるため保持され、`living_room.obj` も今回の範囲では除外しない。

## 検証結果

- 固定参照・名前の重複・相対include・JSONコメント・動的参照・不正JSON・Prefabファイル／ディレクトリ除外等の11テストが成功。
- 実配布スクリプトの合成データテストで、ReleaseからのPrefab除外、Developmentでの保持、元ファイルの保持を確認。
- 再生成したJobHuntingGame配布物で、Prefabの実ファイルとマニフェスト内のPrefabがともに0件であることを確認。
- 日本語の別フォルダーへ移した配布物で全4シーンとTitleScene再起動を確認。各シーンの初回描画とその後2秒間にError/Criticalなし。目視・長時間プレイの検証は含まない。

起動結果: `Generated/CookedRuntimeRegression-f8cf39760dbc493eba874bd326045322/Results.json`。今回の変更は配布ツールとテスト・文書のみで、エンジン本体の再ビルドは行っていない。
