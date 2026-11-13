# 浮遊光のポイントライト有効・無効比較（2026-10-25）

この文書は自動強度調整を導入する前の固定強度での調査記録。現在の設定は [FireflyMotion.md](FireflyMotion.md) の「所持数に応じたポイントライト強度」を参照。

結論: ポイントライトは機能しており、地面・建物へ照明が加わる。ただし現在の強度・範囲では効果はかなり控えめ。通常画面で見える明るい粒の大部分は、別オブジェクトの発光マーカー。

## 比較条件

- 現在のRelease実行ファイルと最新ゲームスクリプトを使用し、隔離したQA環境で実行。現在のセーブの光336個・建築14件を基に、336灯と最大2046灯を別々に確認。
- 光の移動時刻を120秒に固定。生産・住民・カメラを含む街の更新を撮影前に停止し、各組のカメラ・マーカー・光の位置を同一にする。位置のチェックサムも一致。
- `LightRenderer.SetActive`だけを切り替え、ON→OFF→ONの順に比較。再点灯時に同じ見え方へ戻ることも確認。
- 通常比較: 発光マーカーとポストエフェクトを維持し、ポイントライトだけを切り替える。
- 検証比較: マーカーとポストエフェクトを外し、地面・建物への照明効果を直接比較。
- FHD 1920×1080の画像。測定領域は街全体を囲む矩形(x=560～1329, y=330～859)、770×530画素。背景も含むため、平均値は地面や建物だけの平均値ではない。

## 画素比較

数値は8bit RGB（0～255）。輝度差はRGBへ0.2126/0.7152/0.0722を掛けた画像値の差であり、物理的な照度ではない。

| 灯数 | 表示 | 平均輝度差 ON−OFF | 最大RGBチャンネル差 | 再点灯ONとの最大差 |
|---:|---|---:|---:|---:|
| 336 | 通常 | 0.169 / 255 | 28 | 0 |
| 336 | マーカー・ポストなし | 0.196 / 255 | 12 | 0 |
| 2046 | 通常 | 0.770 / 255 | 38 | 1 |
| 2046 | マーカー・ポストなし | 0.876 / 255 | 44 | 0 |

- 336灯・検証画面の座標(921,548)では、OFFのRGB(166,117,66)がONで(178,125,68)へ変化。
- 2046灯・検証画面の座標(911,542)では、OFFのRGB(163,116,68)がONで(207,143,73)へ変化。
- 336灯の地面の一例(900,650)では、OFFの(28,28,30)に対しONは(28,29,30)。地面上の差は1階調程度の場所も多く、通常の引いたカメラでは判別しづらい。
- 再点灯の画像は336灯では完全一致。2046灯でも通常画面の1画素・1階調だけの差で、検証画面は完全一致。したがってON/OFFの差は、動きやマーカーの変化だけによるものではない。

## 弱く見える理由

`Assets/Application/Scripts/LightTown/TownFireflies.as`の現在値は、半径2.2m・距離減衰2・実行時強度約0.0312～0.048。光は約0.78～2.32mの高さを浮遊する。

`Assets/KashipanEngine/Pipeline/Preset/Shader/Object/ObjectPS.hlsl`では、半径外を除外し、`(1 - 距離 / 半径)^減衰`で減衰させる。高さ1.5mから直下の地面へ届く場合でも、減衰率は約0.10。強度0.04なら照明式へ入る強さは約0.004となり、横方向へ離れるとさらに弱くなる。高い光は地面が半径外になる場合もある。

`TownGround.mat`と`TownFacility.mat`はライティング有効。一方、`TownGridUnlit.mat`は意図通りライティング無効なので、グリッド線自体が明るくなるかどうかはポイントライトの動作確認には使えない。

見た目としては「マーカーは明るいが、周囲を照らす力は弱い」という設定になっている。今回、ゲームの光量・半径・描画設定は変更していない。ユーザーのセーブ・設定も保持。

## 画像・検証データ

- 通常画面336灯: [ON](../../Generated/FireflyLightVerification/current/FireflyCompare-normal-on.png) / [OFF](../../Generated/FireflyLightVerification/current/FireflyCompare-normal-off.png)
- 検証画面336灯: [ON](../../Generated/FireflyLightVerification/current/FireflyCompare-diagnostic-on.png) / [OFF](../../Generated/FireflyLightVerification/current/FireflyCompare-diagnostic-off.png)
- 通常画面2046灯: [ON](../../Generated/FireflyLightVerification/maximum/FireflyCompare-normal-on.png) / [OFF](../../Generated/FireflyLightVerification/maximum/FireflyCompare-normal-off.png)
- 検証画面2046灯: [ON](../../Generated/FireflyLightVerification/maximum/FireflyCompare-diagnostic-on.png) / [OFF](../../Generated/FireflyLightVerification/maximum/FireflyCompare-diagnostic-off.png)
- 全画素比較: `Generated/FireflyLightVerification/ComparisonMetrics.json`
- 撮影・切り替え・位置固定のログ: `Generated/FireflyPointLightingVerification.log`、各画像に対応するJSON
- 再実行用: `Generated/VerifyFireflyPointLighting.py`、`Generated/AnalyzeFireflyLightComparison.py`
