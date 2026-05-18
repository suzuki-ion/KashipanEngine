# 5. シーンにコンポーネントを付与

シーン全体の振る舞い（UI管理、ゲーム進行、スポナーなど）は「シーンコンポーネント」として追加します。

- インターフェース：`ISceneComponent`
- 追加先：`SceneBase`（`AddSceneComponent(...)`）

---

## 例：シーンコンポーネントを追加する

既存コードでは `TitleScene` / `TestScene` 内で多くのシーンコンポーネントが追加されています。

（例：`TestScene` では `EnemyManager`, `BombManager` などを `AddSceneComponent(...)` しています。）

```cpp
#include <KashipanEngine.h>

void MyScene::Initialize() {
    auto comp = std::make_unique<MySceneComponent>();
    AddSceneComponent(std::move(comp));
}
```

---

## ポイント（厳密）

- `SceneBase::Update()` の中で
  - シーンコンポーネントが更新され
  - その後に `OnUpdate()` が呼ばれる

といった順序になります（詳細は `SceneBase.cpp` を参照）。

- シーンコンポーネント側からシーンへアクセスする場合は、
  - `GetOwnerScene()`
  - `GetOwnerContext()`

などの `ISceneComponent` API を用います。

> `ISceneComponent` の厳密なAPIは `Reference/05_SceneComponents.md` を参照してください。
