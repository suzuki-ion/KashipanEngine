# ゲームオブジェクト用コンポーネント（`IObjectComponent2D` / `IObjectComponent3D`）

オブジェクトの振る舞いは `IObjectComponent` 派生として実装し、`Object2DBase` / `Object3DBase` に登録して利用します。

- 基底：`KashipanEngine/Objects/IObjectComponent.h`
- コンポーネントは `Initialize` / `Update` / `Finalize` を持ち、必要ならシェーダ変数バインドにも参加できます。

---

## 公開API（`IObjectComponent`）

- メタ情報
  - `const std::string& GetComponentType() const`
  - `size_t GetMaxComponentCountPerObject() const`
- ライフサイクル（戻り値：`std::optional<bool>`）
  - `virtual std::optional<bool> Initialize()`
  - `virtual std::optional<bool> Update()`
  - `virtual std::optional<bool> Finalize()`
- 描画連携（任意）
  - `virtual std::optional<bool> BindShaderVariables(ShaderVariableBinder* binder)`
  - `virtual std::optional<bool> BindInstancingResources(ShaderVariableBinder* binder, std::uint32_t instanceCount)`
  - `virtual std::optional<bool> SubmitInstance(void* instanceMap, std::uint32_t instanceIndex)`
- 所属コンテキスト
  - `void SetOwnerContext(IObjectContext* context)`（エンジンが登録時に呼ぶ）
  - `IObjectContext* GetOwnerContext() const`（protected）
- 複製
  - `virtual std::unique_ptr<IObjectComponent> Clone() const = 0`

### 2D/3D
- `class IObjectComponent2D : public IObjectComponent`
  - `Object2DContext* GetOwner2DContext() const`（protected）
- `class IObjectComponent3D : public IObjectComponent`
  - `Object3DContext* GetOwner3DContext() const`（protected）

---

## 例：2D コンポーネント（Update のみ）

```cpp
#include <KashipanEngine.h>

class AlwaysRotate2D final : public KashipanEngine::IObjectComponent2D {
public:
    AlwaysRotate2D() : IObjectComponent2D("AlwaysRotate2D", 1) {}

    std::optional<bool> Update() override {
        auto* ctx = GetOwner2DContext();
        if (!ctx) return false;

        // ctx->GetComponent("Transform2D") などで別コンポーネントにアクセスする想定
        // （実際の Transform2D API は該当ヘッダに合わせて実装）
        return true;
    }

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<AlwaysRotate2D>(*this);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 任意
    }
#endif
};
```

> `GetOwner2DContext()` / `GetOwner3DContext()` を使うと、所有オブジェクト名や他コンポーネント取得（`GetComponent` 等）にアクセスできます。

---

## コンポーネント間参照（`ObjectContext`）

`Object2DContext` / `Object3DContext` は、所有者（`Object*Base`）の
- `GetComponent*`
- `GetComponents*`
- `HasComponents*`

を委譲して提供します。

関連：`KashipanEngine/Objects/ObjectContext.cpp`

（例）
```cpp
auto* c = ctx->GetComponent("Health");
auto n = ctx->HasComponents("Collider");
```

※ 取得できるコンポーネント名は各コンポーネントの `GetComponentType()`（= コンストラクタで渡す文字列）です。
