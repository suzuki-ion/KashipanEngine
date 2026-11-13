# スプラッシュのDirect2D移行と検証（2026-10-28）

## 実装

- ロゴ、帯、背景、フェードをDirect2Dで描画。文字はDirectWrite、PNGのデコードはWIC。
- エンジンのDirectX12、シーン、TextureManagerに依存しない専用UIスレッドを維持。
- HWNDレンダーターゲットを明示的にHARDWARE指定で作成。失敗した場合のみSOFTWARE指定で再作成。
  `SplashScreen::GetRenderingBackend()` で選択した描画方式を確認可能。
- Win32ウィンドウの背景を先に描画し、メッセージ経由でCOM・Direct2D・DirectWrite・WICと画像の準備を開始。
  d2d1.dllとdwrite.dllは全構成でDelay Load指定。Release実行ファイルのインポート表でも確認。
- 初期化が1秒以内なら既存の冒頭背景時間に重ねる。1秒を超えた場合はロゴ出現を途中から再生せず、準備完了後に出現を開始。
- 画像はGPUへ一度アップロードして再利用。大きなPNGは表示サイズへWICのFant補間で事前縮小し、輪郭の品質も維持。
  モニターサイズ変更時は描画先のResizeと画像リソースの更新を実施。
- 高精度の単調時計と待機可能タイマーで60FPSを目標に更新。ビジーウェイトやtimeBeginPeriodは使用しない。
  通常のWM_TIMER依存を外し、タイトルバーをドラッグ中のOSのモーダルループ内だけWM_TIMERを使用。
- リソース再生成時も、デコード済みのCPU画像・フォント・ファクトリーを再利用。
  EndDraw/Resize失敗時はターゲット依存リソースを破棄し、UIスレッドで再作成。
- ウィンドウを最小化・完全に隠している間は描画を抑制し、演出の終了判定は継続。
- 通常ウィンドウ、1920x1080の左下原点、モニターに応じた解像度、読み込み待ち回転と終了条件は維持。

## ビルドと機能検証

- Development / Release x64 ビルド成功。環境のPDB不一致のため、検証用の子プロセスだけ
  `_CL_=/Z7` と `_LINK_=/DEBUG:NONE` を使用。リポジトリのPDB設定は変更していません。
- 時刻・座標・解像度の回帰テスト成功。
- WICソフトウェアターゲット上で、実際のDirect2D描画関数のHD/FHD配置、帯、回転方向をピクセル検証。
- HWNDターゲットでハードウェア指定による作成成功を確認。
  リソースの明示破棄→再生成、Resize、明示的なソフトウェアへの切り替えも成功。
  これは実GPUの抜き差し・ドライバーリセットを再現した試験ではありません。
- 独立ウィンドウの早期完了、遅延完了、内部中断、素材不在、閉じる操作、最小化中の終了が成功。
- 隔離した実エンジンでも、起動直後と2.3秒後の閉じる要求で終了コード0を確認。
- 同じ描画関数で作成したHDのPNGプレビューを確認。これらはWICソフトウェアターゲットの画像であり、
  GPUウィンドウを撮影したスクリーンショットではありません。
- 複数物理モニター間のドラッグ、高DPIでの実操作は未検証。

## A/B計測

旧GDI+版とDirect2D版の計測用Releaseを、同じ隔離したJobHuntingGame（TitleScene）と
1280x720の描画領域で各2回起動。2回目はバックエンドの順番を逆転しました。
単体試験では、メインスレッドを8秒待機させて完了通知します。
ファイルキャッシュや他アプリの負荷・クロックは固定していません。
旧版エンジン1回目は新しい隔離パスへの初回起動で、キャッシュの準備が発生しているため、
エンジン起動全体の時間短縮をこの試験から断定しません。

FPSはWM_PAINT処理開始の間隔から算出したアプリ側の更新頻度です。画面走査・DWM表示完了FPSではありません。
描画時間はCPU側の処理経過時間で、Direct2DではGPUへのコマンド発行・EndDrawを含みますが、
GPUの実行完了時間を計測したものではありません。

| 描画方式 | 条件 | 試行 | 全表示区間FPS | アニメーション開始後FPS | 待機回転FPS | 待機中の描画平均ms | 待機中の間隔P95 ms |
|---|---|---:|---:|---:|---:|---:|---:|
| d2d | engine | 1 | 57.75 | 59.87 | 59.77 | 0.723 | 17.62 |
| d2d | engine | 2 | 57.40 | 60.00 | 59.99 | 0.736 | 18.04 |
| d2d | standalone | 1 | 58.39 | 59.64 | 59.73 | 0.755 | 17.71 |
| d2d | standalone | 2 | 59.00 | 60.00 | 60.00 | 0.652 | 17.45 |
| gdi | engine | 1 | 8.80 | 7.64 | 7.20 | 138.393 | 207.15 |
| gdi | engine | 2 | 13.04 | 9.41 | 7.54 | 132.903 | 176.93 |
| gdi | standalone | 1 | 13.12 | 10.67 | 9.46 | 105.278 | 126.29 |
| gdi | standalone | 2 | 14.27 | 11.58 | 10.16 | 98.177 | 119.97 |

Direct2D版では全試行でハードウェア指定のターゲット作成成功を記録しました。
アニメーション開始後は59.6〜60.0FPS、実エンジンの待機回転中は59.8〜60.0FPSでした。
初期化中の背景表示も含めた全区間では57.4〜59.0FPSになります。
これは今回の環境での結果であり、ソフトウェアへのフォールバック時のFPS保証ではありません。

## 初期化と表示までの時間

プロセス開始前の親側時刻から、対象PIDのスプラッシュがIsWindowVisibleになった時点までをポーリング。
Direct2D版の背景・準備時間は、Show呼び出しを基準に同じ高精度時計で記録しています。
「準備」にはCOM、ファクトリー、画像のデコード・縮小・アップロード、文字リソースを含みます。
実際のモニターへの表示完了までの測定ではありません。

| 条件 | 試行 | プロセス開始→可視状態ms | Show→背景描画完了ms | Show→描画準備完了ms |
|---|---:|---:|---:|---:|
| standalone | 1 | 192.10 | 57.39 | 252.70 |
| engine | 1 | 500.78 | 79.60 | 347.99 |
| standalone | 2 | 106.04 | 57.80 | 205.80 |
| engine | 2 | 367.16 | 51.51 | 230.71 |

今回の準備時間は約0.21〜0.35秒で、冒頭の1秒の背景時間内に収まりました。
他の環境でも同じ時間で初期化できると保証するものではありません。

## 記録

- [Results.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/Results.json)
- [Runs.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/Runs.json)
- [MeasureAB.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/MeasureAB.py)
- [AnalyzeAB.py](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/AnalyzeAB.py)
- [SplashScreenInstrumented.cpp](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/SplashScreenInstrumented.cpp)
- [ReleaseImports.txt](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/ReleaseImports.txt)
- [SettingsVerification.json](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashD2DFps/SettingsVerification.json)

各フレームのCSVと初期化時間のJSONは同じGenerated/StartupSplashD2DFps配下にあります。
