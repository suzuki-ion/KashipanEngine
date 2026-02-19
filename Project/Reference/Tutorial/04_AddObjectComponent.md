# 4. オブジェクトにコンポーネントを付与

オブジェクトの振る舞いは「オブジェクトコンポーネント」として追加します。

- 2D：`IObjectComponent2D`
- 3D：`IObjectComponent3D`

追加は `Object2DBase` / `Object3DBase` の `RegisterComponent(...)` を使います。

---

## 4.1 エンジン標準コンポーネントを付ける例

例：`Model` オブジェクトに移動コンポーネント（アプリ側実装の `PlayerMove`）を付ける

```cpp
#include <KashipanEngine.h>
#include "Objects/Components/Player/PlayerMove.h"

// obj は Object3DBase 派生（例：Model）
obj->RegisterComponent<PlayerMove>(2.0f, 0.25f);

if (auto* pm = obj->GetComponent3D<PlayerMove>()) {
    pm->SetInputCommand(GetInputCommand());
}
```

---

## 4.2 自作コンポーネントを付ける例

```cpp
#include <KashipanEngine.h>

class Spin final : public KashipanEngine::IObjectComponent3D {
public:
    Spin() : IObjectComponent3D("Spin") {}

    void Update() override {
        auto* owner = GetOwnerObject3D();
        if (!owner) return;
        if (auto* tr = owner->GetComponent3D<KashipanEngine::Transform3D>()) {
            auto r = tr->GetRotate();
            r.y += 1.0f * KashipanEngine::GetDeltaTime();
            tr->SetRotate(r);
        }
    }

    std::unique_ptr<IObjectComponent> Clone() override {
        return std::make_unique<Spin>(*this);
    }
};

// 追加
obj->RegisterComponent<Spin>();
```

> `IObjectComponent2D/3D` の厳密な API は `Reference/03_ObjectComponents.md` を参照してください。
