# 浮遊光の強度段階比較（2026-10-25）

この文書は自動強度調整を導入する前の固定強度での調査記録。現在の設定は [FireflyMotion.md](FireflyMotion.md) の「所持数に応じたポイントライト強度」を参照。

半径2.2mを維持した場合、現在の336灯では16倍前後、最大2046灯では4～8倍から照明の変化がはっきり見える。現在値を一律で大きくする場合は、光の数が増えた際に照明が重なる点への配慮が必要。

## 方法

- 現在のReleaseと最新スクリプト、現在の街のセーブ（336光・建築14件）を隔離QAへコピー。さらに光だけを2046個にしたケースも撮影。
- 光の位置・明滅時刻を120秒に固定。カメラ・生産・住民などの街の更新を撮影前に停止。各画像の位置チェックサムを確認。
- 半径2.2m・減衰2・色・ライト数・マーカーの大きさと明るさは固定。各ライトの現在の明滅強度へ、0 / 1 / 2 / 4 / 8 / 16 / 32 / 64倍を乗算。最後に1倍へ戻して再撮影。
- 通常のマーカー・ポストエフェクト付き画面と、マーカー・ポストエフェクトを外した照明確認用画面を撮影。各ケース18枚、合計36枚。
- 全ライトの最小／最大半径が2.2m、稼働数と位置が各段階で変わらないことを実行時に確認。
- ゲームの強度設定は変更せず、調査のみ実施。ユーザーのセーブと設定を保持。

## 目視結果（通常表示）

倍率は現在の明滅を維持したまま掛ける値。強度の範囲は各ライトの明滅位相を含む概数。

| 倍率 | 強度の範囲 | 336灯 | 2046灯 |
|---:|---:|---|---|
| 1倍 | 0.031～0.048 | 現在の見た目。照明効果は弱い | 現在の見た目。照明の重なりはある |
| 2倍 | 0.062～0.096 | ほぼ控えめ | 少し明るくなる |
| 4倍 | 0.125～0.192 | 建物の照らされ方に差が出始める | 変化がはっきり分かる |
| 8倍 | 0.250～0.384 | 照明差が分かるが、引いた画面ではまだ控えめ | かなりはっきり明るくなる |
| 16倍 | 0.499～0.768 | 建物・地面の変化がはっきり分かる | 建物の一部が強く光って見える |
| 32倍 | 0.998～1.536 | かなり明るく、屋根などが目立つ | 強すぎる部分や白に近い画素が目立ち始める |
| 64倍 | 1.997～3.072 | 一部がかなり明るくなる | 強すぎる。白に近い画素や色の飽和が増える |

### 調整候補

- 現在の336灯で「かなり変わった」と分かることを優先するなら、16倍（強度約0.50～0.77）が目安。32倍では建物の局所的な明るさがかなり強くなる。
- 最大2046灯まで考慮するなら、4倍（約0.125～0.192）で変化が分かり、8倍（約0.25～0.38）でかなり強くなる。16倍以上は建物の局所的な明るさが強すぎる傾向。
- 一律設定を試す出発点なら4～8倍が扱いやすい。ただし336灯での変化は、16倍より控えめになる。
- 目視評価は今回の街・固定カメラ・光の配置に対する判断。施設が密集する配置や別の光の高さでは変わる。全マップに対する固定の閾値ではない。

## 原画像の画素比較

FHD画像の街を囲む同一矩形（x=560～1329, y=330～859、770×530画素）を比較。背景・グリッドも含む。平均値は地面だけの平均ではない。RGBへ0.2126/0.7152/0.0722を掛けた画像上の輝度差であり、物理的な照度ではない。

「差5以上」は現在の1倍より輝度が5/255以上上がった画素の割合。人間の視認閾値を意味しない。「250以上」は現在は250未満だったRGBチャンネルのいずれかが250以上へ上がった画素の割合で、飽和に近い部分の目安。

| 倍率 | 336灯：平均輝度差 /255 | 336灯：差5以上 | 2046灯：平均輝度差 /255 | 2046灯：差5以上 | 2046灯：新規250以上 |
|---:|---:|---:|---:|---:|---:|
| 1倍 | 0.00 | 0.0% | 0.00 | 0.0% | 0.000% |
| 2倍 | 0.12 | 0.0% | 0.77 | 0.9% | 0.000% |
| 4倍 | 0.36 | 0.4% | 2.34 | 17.7% | 0.002% |
| 8倍 | 0.88 | 2.5% | 5.63 | 39.0% | 0.179% |
| 16倍 | 1.94 | 13.7% | 12.10 | 43.2% | 0.393% |
| 32倍 | 4.20 | 26.9% | 26.47 | 45.7% | 0.575% |
| 64倍 | 8.99 | 35.7% | 52.82 | 47.8% | 4.890% |

1倍へ戻した画像は、両灯数・両表示モードとも比較領域で全画素一致。変化が動きや時間経過によるものではないことを確認した。数値は原解像度PNGから計算し、比較表示用の縮小画像からは計算していない。

## 原画像

### 336灯

- 1倍: [通常表示](../../Generated/FireflyIntensitySweep/current/Intensity-normal-1.png) / [照明のみ](../../Generated/FireflyIntensitySweep/current/Intensity-diagnostic-1.png)
- 8倍: [通常表示](../../Generated/FireflyIntensitySweep/current/Intensity-normal-4.png) / [照明のみ](../../Generated/FireflyIntensitySweep/current/Intensity-diagnostic-4.png)
- 16倍: [通常表示](../../Generated/FireflyIntensitySweep/current/Intensity-normal-5.png) / [照明のみ](../../Generated/FireflyIntensitySweep/current/Intensity-diagnostic-5.png)
- 32倍: [通常表示](../../Generated/FireflyIntensitySweep/current/Intensity-normal-6.png) / [照明のみ](../../Generated/FireflyIntensitySweep/current/Intensity-diagnostic-6.png)
- 64倍: [通常表示](../../Generated/FireflyIntensitySweep/current/Intensity-normal-7.png) / [照明のみ](../../Generated/FireflyIntensitySweep/current/Intensity-diagnostic-7.png)

### 2046灯

- 1倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-1.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-1.png)
- 4倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-3.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-3.png)
- 8倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-4.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-4.png)
- 16倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-5.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-5.png)
- 32倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-6.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-6.png)
- 64倍: [通常表示](../../Generated/FireflyIntensitySweep/maximum/Intensity-normal-7.png) / [照明のみ](../../Generated/FireflyIntensitySweep/maximum/Intensity-diagnostic-7.png)

## 再現・検証データ

- `Generated/SweepFireflyLightIntensity.py`
- `Generated/FireflyIntensitySweep.log`
- `Generated/FireflyIntensitySweep/IntensityMetrics.json`
- 各撮影画像に対応するJSON（倍率・実強度の最小最大・半径・点灯数・位置チェックサム）
- `Generated/AnalyzeFireflyIntensitySweep.py`
