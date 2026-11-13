# 起動スプラッシュのFPS計測（2026-10-28）

## 結論

HD描画（1280x720）でも、スプラッシュ単体は全体平均16.6〜17.8 FPS、
実エンジン起動時は全体平均14.0〜19.5 FPSでした。
特に、実エンジンの読み込み待ち回転中は10.3〜11.5 FPSで、見た目だけでなく描画更新自体が低頻度です。

待機回転中の1フレームは平均85.4〜96.4msで、その内訳はGDI+でのフレーム生成67.4〜74.7ms、
画面転送18.0〜21.6msです。60FPSの1フレーム予算約16.7msを大きく超えます。
今回の測定では描画処理に時間の大部分が使われており、タイマーだけの変更ではなく描画処理の軽量化が必要と判断します。
画像ごとの個別コスト、GPU実行時間、DWMの最終表示FPSは計測していません。

## 条件と計測方法

- 現在のスプラッシュ実装をGenerated配下へ複製し、QueryPerformanceCounterによる計測だけを追加。
- Release相当の `/O2 /GL /MT` でビルド。実エンジン側は既存のReleaseオブジェクトを使い、
  スプラッシュのオブジェクトだけ計測版に差し替えて別のexeへリンク。
- 通常のエンジンexe・ソースには計測コードを入れていません。
- スプラッシュ単体はメインスレッドを8秒待機させてから完了通知。
- 実エンジンは現在選択中のJobHuntingGameのAssetsを隔離したスタンドアロン配置へコピー。
  起動シーンはTitleScene。元プロジェクトの保存データ・設定を測定先にしません。
- 単体→エンジンを交互に3回ずつ。OSキャッシュは消していません。
  エンジン1回目は隔離先の初回起動であり、制御されたコールド起動ではありません。
- 全試行の描画領域は1280x720。通常の他アプリの負荷やCPU/GPUクロックは固定していません。
- フレーム間隔はWM_PAINTの処理開始間のQPC差。
  FPSは `(フレーム数-1) / 開始時刻の差` で算出し、瞬間FPSの算術平均にはしません。
- 「生成」はDrawLogoFrame（背景、PNGの拡縮・回転、文字、フェード）にかかった経過時間。
  「転送」はその終了からHDC転送とGraphics破棄の終了まで。
  「描画全体」はBeginPaint前からEndPaint終了まで。
- UIスレッド上のメモリへ記録し、終了時に一括でCSV化。フレームごとのファイル書き込みは行いません。
  計測用コードの負荷は結果に含まれます。
- 各試行はCSVの書き込み完了を確認してから測定用プロセスのみ終了。
  実エンジンの通常終了を検証する試験ではありません。

## 全表示区間

背景のみの最初の1秒と、最後のフェードも含みます。

| 条件 | 試行 | フレーム数 | 平均FPS | 平均間隔ms | 間隔P95 ms | 最大間隔ms | 生成平均ms | 転送平均ms | 描画全体平均ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 単体 | 1 | 156 | 17.20 | 58.15 | 78.98 | 95.81 | 40.08 | 16.11 | 56.23 |
| 単体 | 2 | 162 | 17.82 | 56.12 | 74.87 | 86.84 | 38.67 | 15.34 | 54.06 |
| 単体 | 3 | 150 | 16.56 | 60.38 | 90.88 | 95.71 | 41.36 | 16.64 | 58.05 |
| 実エンジン | 1 | 211 | 13.98 | 71.51 | 103.18 | 210.21 | 50.61 | 19.34 | 70.01 |
| 実エンジン | 2 | 99 | 19.51 | 51.25 | 91.00 | 121.96 | 30.85 | 17.99 | 48.90 |
| 実エンジン | 3 | 107 | 15.03 | 66.52 | 108.57 | 136.45 | 42.87 | 22.01 | 64.96 |

## 読み込み待ちの回転中

`loading=true` かつフェード開始前のフレームのみ。
この区間の集計では、前後のフレームが両方とも条件に該当する間隔だけを使います。

| 条件 | 試行 | フレーム数 | 平均FPS | 間隔P95 ms | 最大間隔ms | 生成平均ms | 転送平均ms | 描画全体平均ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 単体 | 1 | 67 | 13.30 | 80.81 | 95.81 | 59.53 | 15.42 | 74.99 |
| 単体 | 2 | 70 | 13.95 | 75.82 | 86.84 | 56.77 | 14.81 | 71.61 |
| 単体 | 3 | 63 | 12.54 | 93.84 | 95.71 | 62.50 | 17.17 | 79.70 |
| 実エンジン | 1 | 125 | 11.30 | 108.74 | 210.21 | 67.61 | 20.58 | 88.25 |
| 実エンジン | 2 | 12 | 11.53 | 109.71 | 121.96 | 67.36 | 18.04 | 85.43 |
| 実エンジン | 3 | 31 | 10.28 | 129.28 | 136.45 | 74.71 | 21.65 | 96.42 |

## 読み込み完了前の区間

最初のシーン描画完了の通知がUIスレッド側へ見えた時点を境界としているため、通知検出に最大1フレーム程度の誤差があります。
起動全体の絶対時間ではなく、スプラッシュ最初の描画開始からの相対時間です。

| 試行 | 完了通知を検出した相対時刻ms | 通知前平均FPS | 通知前描画全体平均ms |
|---|---:|---:|---:|
| 1 | 13342.56 | 14.03 | 69.69 |
| 2 | 3957.13 | 20.89 | 44.85 |
| 3 | 5416.58 | 15.76 | 61.70 |

## 補足

- ソースは16msのSetTimerを指定していますが、今回の1フレームの描画全体はそれより長くかかっています。
- 待機回転中に特に重くなっていますが、回転だけの追加コストは個別に計測していないため、
  歯車だけが原因とは断定しません。背景・パン・文字の再描画、補間、画面転送も含まれています。
- 元PNGは歯車1024x1024、パン1024x1024、文字2048x512です。
- アニメーション時刻の更新幅には31/62/78/94ms等が多く見られます。
  GetTickCount64の粒度に加え、描画の間隔自体が長いためです。
- ここでいうFPSはアプリ側の描画更新頻度であり、モニターのリフレッシュレートや
  DWMの表示完了・画面走査を保証する数値ではありません。

## 再現用の記録

- [Results.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/Results.json)
- [Runs.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/Runs.json)
- [InputHashes.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/InputHashes.json)
- [InputVerification.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/InputVerification.json)
- [Measure.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/Measure.py)
- [Analyze.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/Analyze.py)
- [BuildProbe.cmd](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/BuildProbe.cmd)
- [SplashScreenInstrumented.cpp](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/SplashScreenInstrumented.cpp)
- [engine-1.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/engine-1.csv)
- [engine-2.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/engine-2.csv)
- [engine-3.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/engine-3.csv)
- [standalone-1.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/standalone-1.csv)
- [standalone-2.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/standalone-2.csv)
- [standalone-3.csv](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashFps/standalone-3.csv)

ソース2ファイル、UserSettings/Settings.json、JobHuntingGame/Project.jsonの計測前後のSHA-256一致を確認しました。
