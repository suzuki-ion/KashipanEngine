# デバッグ用ログ出力（Log / LogScope）

対象コード:

- `KashipanEngine/Debug/Logger.h`（API）
- `KashipanEngine/KashipanEngine.cpp`（初期化/終了）
- `KashipanEngine/Debug/LogSettings.*`（ログ設定のロード）

## 1. 初期化と終了

`KashipanEngine::Execute`（`KashipanEngine/KashipanEngine.cpp`）内で以下が呼ばれます。

- `LoadLogSettings({}, logSettingsPath);`
- `InitializeLogger({});`

エンジン終了時:

- `ShutdownLogger({});`

クラッシュハンドラ用:

- `ForceShutdownLogger(PasskeyForCrashHandler);`

## 2. ログ種類

`Logger.h` に以下の列挙があります。

- `LogDomain`
  - `GameEngine`
  - `Application`

- `LogSeverity`
  - `Debug`
  - `Info`
  - `Warning`
  - `Error`
  - `Critical`

## 3. ログ出力 API

- `void Log(const std::string &logText, LogSeverity severity = LogSeverity::Info);`
- `void LogSeparator();`

`Log` は任意の文字列を出力します。

## 4. LogScope

`LogScope` は「スコープに入った/出た」をプレフィックスとしてスタック管理するための RAII です。

- コンストラクタで `PushPrefix(location)`
- デストラクタで `PopPrefix()`

`location` は `std::source_location`（デフォルトで `current()`）で、

- ファイル
- 行
- 関数名

等の情報を利用できる設計です。

コード上では以下のように使われています（例）:

- `EngineSettings.cpp`
  - `LogScope scope;`
  - `Log("...", LogSeverity::Info);`

## 5. 運用メモ

- 関数冒頭で `LogScope scope;` を置くと、ログに呼び出し階層が乗る運用ができます。
- 重要度に応じて `LogSeverity` を使い分けます。
- ログテキストに翻訳を使う場合は `Translation(key)` を併用します（`Logger.h` は `Translation.h` を include）。
