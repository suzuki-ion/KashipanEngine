# CPUプロファイリングとReleaseデバッグ表示

Debug / Developmentでは「デバッグウィンドウ」メニューの「エンジンのプロファイリング」で表示を切り替えます。ウィンドウ右上の閉じるボタンからも非表示にできます。

処理別に直近の完了フレームの時間、直近1〜600フレームの平均・最大時間、直近フレームの呼び出し回数を表示します。初期値は60フレームです。呼び出されなかったフレームは0として平均へ含めます。更新、入力、音声、動画、シーン、コンポーネント型ごとの更新、描画リスト構築、Compute、スキニング、パーティクル、ライトカリング、シャドウ、描画先ごとの処理、Present/GPU待機、シーン切り替え・破棄を計測します。

FPSは完了フレーム間の実時間から計算し、ゲームのデルタタイム上限やシーン切り替えによるデルタタイムリセットの影響を受けません。

時間はCPU側の経過時間です。GPU上でシェーダーが実行された時間ではありません。入れ子の計測は子の時間を含むため、各行の合計はフレーム時間と一致しません。`Draw.SubmitPresentWait`にはPresentと同期の待機を含みます。

ReleaseではゲームウィンドウをアクティブにしてF12を押すと画面左上にFPSとプロファイリング結果を表示し、もう一度押すと非表示にします。キーを押し続けても繰り返し切り替わりません。表示は入力を取りません。複数ウィンドウがある場合は最後にアクティブだったエンジンのウィンドウへ表示します。画面の高さに収まらない結果は平均時間の大きい順に表示し、表示件数を併記します。

`Project/Application/AppInitialize.h`の`AppInitialize`から設定できます。初期状態は非表示で、FPSとプロファイリングの両方を有効にしています。

```cpp
context.SetDebugTextToggleKey(Key::F11); // 既定はKey::F12。Key::Unknownでキー切り替えを無効化
context.SetDebugTextContent(true, true); // FPS, プロファイリング
context.SetDebugTextVisible(false);
context.SetProfilingWindowVisible(true); // エディターのウィンドウ表示
Profiler::GetInstance().SetSampleCount(120);
```

アプリケーション独自の処理にはスコープを追加します。

```cpp
#include "Debug/Profiler.h"

void UpdateEnemies() {
    KashipanEngine::Profiler::Scope scope("Application.UpdateEnemies");
    // 計測する処理。returnや例外でもスコープ終了時に記録する。
}
```

同じ名前のスコープはフレーム内で合算します。計測は表示の有無にかかわらず継続します。名前は固定文字列やコンポーネント型のように種類を限定し、オブジェクトIDなどを埋め込まないでください。保持する名前はプロセス全体で最大256種類です。別スレッドからの記録は同期されますが、フレーム境界をまたぐスコープとゲームループの外のスコープは記録しません。非同期処理のCPU時間はメインスレッドの時間と重なる場合があります。

回帰テストはVisual Studio x64 Developer PowerShellで`Project/Tests/RunProfilerRegression.ps1`を実行します。実際の画面表示はDebug / Developmentのウィンドウ開閉とReleaseのF12、キー変更、リサイズ、シーン切り替えを確認してください。
