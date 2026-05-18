# `AppInitialize`（アプリ側初期化）

`GameEngine` は起動時に `AppInitialize(context)` を呼び出し、アプリ側で以下を行う想定です。

- ウィンドウ生成（`Window::CreateNormal` / `Window::CreateOverlay`）
- シーン登録（`SceneManager::RegisterScene`）と初期シーンへの遷移（`ChangeScene`）
- 入力マッピング（`InputCommand::RegisterCommand`）
- 必要ならゲームループ終了条件の差し替え（`context.SetGameLoopEndCondition`）

関連実装：`Application/AppInitialize.h`

---

## 公開API（利用側が触るもの）

### `GameEngine::Context`
- `SceneManager* sceneManager`
- `InputCommand* inputCommand`
- `void SetGameLoopEndCondition(const std::function<bool()> &func) const`

### `Window`
- `static Window* CreateNormal(...)`
- `static Window* CreateOverlay(...)`

### `SceneManager`
- `template<typename TScene, typename... Args> void RegisterScene(const std::string& sceneName, Args&&... args)`
- `bool ChangeScene(const std::string& sceneName)`

### `InputCommand`
- `void Clear()`
- `void RegisterCommand(...)`（キーボード/マウス/パッドなど複数オーバーロード）

---

## 例：基本形（ウィンドウ作成＋シーン登録＋初期シーン）

```cpp
#include <KashipanEngine.h>
#include "Scenes/MyScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    Window::CreateOverlay("3104_グランナー", 1280, 720, true);

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        sm->RegisterScene<MyScene>("MyScene");
        sm->ChangeScene("MyScene");
    }
}

} // namespace KashipanEngine
```

---

## 例：ゲームループ終了条件の上書き

デフォルトの終了条件は「Window が 0 個になったら終了」です。アプリ側で差し替え可能です。

```cpp
inline void AppInitialize(const GameEngine::Context &context) {
    context.SetGameLoopEndCondition([]{
        // 任意の終了条件を返す
        return Window::GetWindowCount() == 0;
    });
}
```
