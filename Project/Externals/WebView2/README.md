# WebView2

プロジェクトランチャー（KashipanHub）のUIをHTML/CSSで描画するために使用しているほか、
KashipanEngine本体の起動時スプラッシュ画面（`KashipanEngine/Splash/SplashScreen`）でも
使用しています。スプラッシュ画面は`KashipanEngine.cpp`の起動処理でエディター/ランタイム
（`--project`起動）を問わず常に表示されるため、**KashipanEngine.exeを使って配布する
ゲームにも`WebView2Loader.dll`が同梱されます。**

- 取得元: NuGet `Microsoft.Web.WebView2`
- バージョン: 1.0.4078.44
- 収録内容: `build/native/include` のヘッダー2点と、`build/native/x64` の
  ローダー（DLLとインポートライブラリ）のみ。x86/arm64 と WinRT 版は使わないため除外しています。

## 実行に必要なもの

ヘッダーとローダーはビルドに必要なだけで、実際の描画には
**WebView2 ランタイム**（別途OSにインストールされているもの）が使われます。
Windows 11 には標準で同梱されています。

ランタイムが無い環境では WebView2 の初期化に失敗しますが、その場合ランチャーは
Win32コントロールで組んだ簡易UIへ自動的に切り替わるため、起動できなくなることはありません。

## 更新方法

新しいバージョンへ差し替える場合は、nupkg を展開して上記4ファイルを置き換えてください。
