# KashipanEngine.h リファレンス

`KashipanEngine/KashipanEngine.h` は、エンジン利用側が **1つ include するための集約ヘッダ** です。

```cpp
#include <KashipanEngine.h>
```

内部では以下のヘッダ群をまとめて include します。

- `AssetsHeaders.h`
- `CoreHeaders.h`
- `EngineSettings.h`
- `GraphicsHeaders.h`
- `InputHeaders.h`
- `MathHeaders.h`
- `ObjectsHeaders.h`
- `SceneHeaders.h`
- `UtilitiesHeaders.h`

## エントリポイント

`KashipanEngine` 名前空間に以下が定義されています。

- `int Execute(PasskeyForWinMain, const std::string &engineSettingsPath);`

`main.cpp` 例（リポジトリ内の実装例）:

- `main.cpp`
  - `WinMain(...) { return KashipanEngine::Execute({}, "Assets/KashipanEngine/EngineSettings.json"); }`

## Execute が行うこと（`KashipanEngine/KashipanEngine.cpp`）

`Execute` はエンジン全体の起動～終了を行います。

1. クラッシュハンドラ設定
   - `SetUnhandledExceptionFilter(CrashHandler);`
2. DX12 リソースリークチェッカー（スコープで監視）
   - `D3DResourceLeakChecker resourceLeakChecker;`
3. エンジン設定 JSON をロード
   - `LoadJSON(engineSettingsPath)`
4. ログ設定とロガー初期化
   - `LoadLogSettings({}, logSettingsPath);`
   - `InitializeLogger({});`
5. エンジン設定反映
   - `LoadEngineSettings({}, engineSettingsPath);`
6. `GameEngine` を生成して実行
   - `std::unique_ptr<GameEngine> engine = std::make_unique<GameEngine>(PasskeyForGameEngineMain{});`
   - `int code = engine->Execute({});`
7. 終了処理
   - `engine.reset();`
   - `ShutdownLogger({});`

## EngineSettings

エンジン設定は `EngineSettings.h/.cpp` にあります。

- `LoadEngineSettings(PasskeyForGameEngineMain, engineSettingsPath)`
- `GetEngineSettings()`

主な設定構造体（`EngineSettings`）:

- `EngineSettings::Window`
  - `initialWindowTitle`, `initialWindowWidth`, `initialWindowHeight`, `initialWindowStyle`, `initialWindowIconPath`
- `EngineSettings::Limits`
  - `maxTextures`, `maxSounds`, `maxModels`, `maxGameObjects`, `maxComponentsPerGameObject`, `maxWindows`
- `EngineSettings::Rendering`
  - `defaultClearColor`, `defaultEnableVSync`, `defaultMaxFPS`, `pipelineSettingsPath`, 各種 descriptor heap size
- `EngineSettings::Translations`
  - `languageFilePaths`
