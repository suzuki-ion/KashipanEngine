# 元アセットを含めないRelease配布

Releaseの配布処理は、配布用ステージングへコピーした画像・モデル・シェーダーを事前変換し、`Cooked/Manifest.json` と `Cooked/*.cache` を同梱する。元の `GameProjects/*/Assets` は変更しない。Development配布と通常の開発時キャッシュは従来の動作を維持する。

事前変換の前に[固定参照解析](ReleaseAssetAnalysis.md)で未使用と判断できたアセットを除外する。PrefabはRelease配布から一律に除外し、配布用マニフェストにも含めない。

```powershell
.\EngineTools\CreateDistribution.ps1 `
    -ProjectDir . -TargetDir .\Generated\Outputs\x64\Release `
    -Platform x64 -Configuration Release
```

通常のReleaseビルドのPostBuildでもこの処理が実行される。従来の元アセット付き配布が必要な場合は、手動実行時に `-KeepSourceAssets` を指定する。

## 配布するもの

| 対象 | 事前生成データ | 配布時に除去する元ファイル |
|---|---|---|
| 通常画像、DDS、HDR | DDS（必要な形式変換とミップマップ生成済み） | 変換成功した画像 |
| モデル | 既存ModelSceneCache形式の解析済みシーン（骨・アニメーション・埋め込み画像も含む） | 変換成功したモデルと、実際に読み込んだ外部 `.bin` / `.mtl` |
| シェーダー | DXILとMaterialLayout | `.hlsl` / `.hlsli`。配布用DXILにはデバッグソースを埋め込まない |

GIF、音声、動画、フォント、AngelScript、シーン、マテリアル、パイプラインJSON等は引き続き必要な配布アセットとして残す。全ファイルを単一アーカイブにする機能ではない。

マニフェストはAssetsからの相対パスでアセット一覧と生成物を参照する。元画像・元モデル・HLSLや開発PCの絶対パスへの依存検証を行わず、既存のキャッシュコンテナーで生成物自体のSHA-256と形式を検証する。モデルのスキーマ・スカラー型のサイズも確認する。エンジン実行ファイルと生成データは一式で配布する。

PSOは同梱しない。各PCで従来の `Cache/Startup/PSOs`（書き込めない場合はLOCALAPPDATA）へ生成する。`Cooked` は読み取り専用でも使用できる。

## 実行中に合成するシェーダー

シーン・マテリアル・スクリプト等の文字列中に記載された `Object3D.*` / `Object2D.*` バリアントを探索し、モジュール合成、コンパイル、マテリアル情報の保存を配布時に行う。通常のプリセットと静的パイプラインのシェーダーも事前コンパイルする。

実行時に文字列の連結等で作るモジュール組み合わせは自動で推測できないため、プロジェクトの `Assets/CookSettings.json` へ必要な名前を列挙する。

```json
{
  "pipelineVariants": [
    "Object3D.Dissolve.Solid.BlendNormal"
  ]
}
```

ソースが不要なBlend/Cullingの変更は引き続き実行時に解決できる。未生成のモジュール組み合わせ・シェーダーマクロの組み合わせは、HLSLへフォールバックせずエラーにする。

## 検証

VS x64開発者PowerShellで以下を実行する。

```powershell
.\EngineTests\RunCookedAssetsRegression.ps1
python .\EngineTests\RunCookedDistributionRegression.py .\Generated\Distribution\x64\Release
```

前者は元ファイルなしの画像・モデル・DXIL・MaterialLayout復元、日本語パスへの移動、未生成・欠損・破損データの拒否を検証する。後者は実配布物を別の日本語フォルダーへコピーし、元ファイル除去と生成物ハッシュを確認したうえで、各登録シーンの初回描画と更新を検証し、`Generated/CookedRuntimeRegression-*/Results.json` に結果を残す。画面の目視確認は別途必要。

### 今回の確認結果

Releaseビルド、既存起動最適化テスト一式、新規CookedAssets回帰テスト、既存配布レイアウトテストが成功した。

JobHuntingGameでは画像47件・モデル49件・固有シェーダー80件を生成した。配布物内の元画像・モデル・HLSLは0件。生成データは約282.27 MiB、残るAssetsは約37.14 MiB（実行ファイル等は別）。

最終配布物を日本語の別フォルダーへ移し、異なる作業ディレクトリからTitleScene・CityScene・ExpeditionScene・StageSelectSceneとTitleSceneの再起動を確認した。全5回で画像47件・モデル49件を復元し、起動時のシェーダー呼び出し119件は全件ヒット、モデル再解析は0件。各シーンの初回描画とその後2秒間にError/Criticalなし。PSOは初回に106件保存し、次の起動では108件すべてヒットした。

結果: `Generated/CookedRuntimeRegression-c361f8708fb44c0ab394d8ecd845b288/Results.json`。これは同一PCでの実行検証であり、別PC・別GPUでの確認、画面の目視、長時間のゲームプレイ、通常終了の検証は含まない。
