# 2. シーンの登録

作成した `SceneBase` 派生クラスは、`SceneManager` に登録してから使用します。

このプロジェクトでは、エンジン初期化の最後に **アプリ側の** `AppInitialize(const GameEngine::Context&)` が呼ばれ、
そこで
- ウィンドウ生成
- シーン登録
- 初期シーンへの切り替え

を行います。

---

## 2.1 実際の `AppInitialize` の場所

- `Application/AppInitialize.h`

このファイルが「アプリ側の初期化ポイント」です。
実装では `context.sceneManager` が有効な場合に `RegisterScene<TScene>(...)` を呼び、最後に `ChangeScene("...")` で初期シーンへ切り替えています。

---

## 2.2 `SceneManager::RegisterScene` の厳密な仕様（実装より）

- `KashipanEngine/Scene/SceneManager.h`

`SceneManager` には以下のテンプレート関数が定義されています。

- `template<typename TScene, typename... Args> void RegisterScene(const std::string &sceneName, Args &&...args);`

この関数の実装上の仕様：

- `TScene` は `SceneBase` 派生である必要があります（`static_assert(std::is_base_of_v<SceneBase, TScene>)`）。
- `args...` は `std::tuple` として factories にキャプチャされ、シーン生成時に `std::apply` で `TScene` のコンストラクタへ転送されます。
- 生成されたシーンには内部で必ず
  - `scene->SetSceneManager(Passkey<SceneManager>{}, sm);`

  が呼ばれ、所有 `SceneManager` が設定されます。

---

## 2.3 `AppInitialize` に追加する「登録コード」の実例（このプロジェクトと同じ書き方）

以下は `Application/AppInitialize.h` と同じスタイルで、
- `MyScene` を登録し
- 初期シーンを `MyScene` にする

実例です。

```cpp
#pragma once
#include <KashipanEngine.h>
#include "Scenes/MyScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    // 1) ウィンドウ生成（このプロジェクトでも AppInitialize 内で作っている）
    Window::CreateNormal("Main Window", 1280, 720);

    // 2) シーン登録と初期シーン設定
    if (context.sceneManager) {
        auto *sm = context.sceneManager;

        sm->RegisterScene<MyScene>("MyScene");
        sm->ChangeScene("MyScene");
    }
}

} // namespace KashipanEngine
```

---

## 2.4 コンストラクタ引数が必要なシーンの登録例（RegisterScene の `args...` を使う）

`RegisterScene<TScene>("Name", args...)` の `args...` は、
シーン生成時に `std::make_unique<TScene>(args...)` の形で転送されます。

例：`MyScene(int level, std::string stageName)` の場合

```cpp
sm->RegisterScene<MyScene>("MyScene", 3, std::string("StageA"));
```

---

## 2.5 既存コード（実例）の参照

このプロジェクトの `Application/AppInitialize.h` では、実際に以下のように登録しています。

- `sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "");`
- `#if DEBUG_BUILD` で `TestScene` の登録
- `context.sceneManager->ChangeScene("EngineLogoScene");`

これをベースに、追加したいシーンを同様に登録してください。
