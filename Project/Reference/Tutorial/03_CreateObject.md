# 3. オブジェクトの作成

KashipanEngine のゲームオブジェクトは、
- 2D：`Object2DBase` 派生
- 3D：`Object3DBase` 派生

または、エンジン標準のプリミティブ/既定オブジェクト（`Sprite`, `Box`, `Model` など）を使って作成します。

---

## 3.1 例：2D の `Sprite` を作る

`Sprite` は `Object2DBase` の派生クラスです。

```cpp
#include <KashipanEngine.h>

auto obj = std::make_unique<KashipanEngine::Sprite>();
obj->SetName("MySprite");
obj->SetPivotPoint(0.5f, 0.5f);
```

---

## 3.2 例：3D の `Box` を作る

```cpp
#include <KashipanEngine.h>

auto obj = std::make_unique<KashipanEngine::Box>();
obj->SetName("MyBox");

if (auto* tr = obj->GetComponent3D<KashipanEngine::Transform3D>()) {
    tr->SetTranslate(KashipanEngine::Vector3(0.0f, 0.0f, 0.0f));
    tr->SetScale(KashipanEngine::Vector3(1.0f));
}
```

---

## 3.3 シーンに追加する

`SceneBase` 派生クラスの中で `AddObject2D` / `AddObject3D` を使ってシーンに所有させます。

```cpp
void MyScene::Initialize() {
    auto obj = std::make_unique<KashipanEngine::Sprite>();
    obj->SetName("HUD");

    AddObject2D(std::move(obj));
}
```

---

## 3.4 描画先に登録する（Window / ScreenBuffer）

オブジェクトは `AttachToRenderer(...)` で描画先に登録します。

```cpp
auto* window = KashipanEngine::Window::GetWindow("Main Window");
if (window) {
    obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
}
```

> `pipelineName` は Assets のパイプライン定義と一致している必要があります。
