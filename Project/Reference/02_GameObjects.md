# ゲームオブジェクト（`Object2DBase` / `Object3DBase`）

KashipanEngine の「オブジェクト」は、
- `Object2DBase`（2D）
- `Object3DBase`（3D）

を基底として作成します。

オブジェクトの振る舞いは「オブジェクトコンポーネント（`IObjectComponent2D` / `IObjectComponent3D`）」として登録し、`Update()` で順に更新されます。

---

## エンジン標準のプリミティブ/既定オブジェクト

エンジン側には、すぐに使えるプリミティブ（既定の頂点/インデックス生成を行う）オブジェクトが用意されています。

### 2D（`KashipanEngine/Objects/GameObjects/2D/*`）
- `Sprite`（`Sprite.h`）
- `Rect`
- `Triangle2D`
- `Ellipse`
- `Line2D`

`Sprite` 例：
```cpp
#include <KashipanEngine.h>

auto sprite = std::make_unique<KashipanEngine::Sprite>();

// アンカーポイント（(0,0)=左上、(0.5,0.5)=中央）
sprite->SetPivotPoint(0.5f, 0.5f);
```

### 3D（`KashipanEngine/Objects/GameObjects/3D/*`）
- `Box`
- `Sphere`
- `Plane3D`
- `Triangle3D`
- `Line3D`
- `Billboard`
- `Model`（モデルローダー連携）

> これらは「ゲームオブジェクト」であり、追加の振る舞いはコンポーネントとして積む運用を想定しています。

---

## 公開API（抜粋）

### `Object2DBase`（`KashipanEngine/Objects/Object2DBase.h`）
- ライフサイクル
  - `void Update()`
  - `void Render()`
- 名前
  - `void SetName(const std::string&)`
  - `const std::string& GetName() const`
- ユーザーデータ
  - `MyStd::AnyUnorderedMap& UserData()` / `const MyStd::AnyUnorderedMap& UserData() const`
- コンポーネント
  - `template<typename T, typename... Args> bool RegisterComponent(Args&&... args)`
  - `bool RegisterComponent(std::unique_ptr<IObjectComponent> comp)`
  - `std::vector<IObjectComponent2D*> GetComponents2D(const std::string&) const`
  - `IObjectComponent2D* GetComponent2D(const std::string&) const`
  - `template<typename T> T* GetComponent2D() const`
  - `size_t HasComponents2D(const std::string&) const`
  - `bool RemoveComponent2D(const std::string&, size_t index = 0)`
- 描画（永続レンダーパス登録）
  - `RenderPassRegistrationHandle AttachToRenderer(Window* targetWindow, const std::string& pipelineName, ...)`
  - `RenderPassRegistrationHandle AttachToRenderer(ScreenBuffer* targetBuffer, const std::string& pipelineName, ...)`
  - `void DetachFromRenderer()` / `bool DetachFromRenderer(RenderPassRegistrationHandle)`
  - `std::vector<RenderPassRegistrationInfo> GetRenderPassRegistrations() const`
- バッチキー
  - `void SetBatchKey(std::uint64_t key, RenderType renderType = RenderType::Instancing)`
  - `void SetUniqueBatchKey()`
  - `std::uint64_t GetInstanceBatchKey() const`

### `Object3DBase`（概念は `Object2DBase` と同様）
- 3D は描画先として `ShadowMapBuffer` もサポートします（`AttachToRenderer(ShadowMapBuffer*, ...)`）。

---

## バッチキー（Batch Key）

### 概要

バッチキーは、同じパイプラインを使用するオブジェクト同士を**まとめて描画（バッチング/インスタンシング）**するための仕組みです。

同じバッチキーを持つオブジェクトは、GPU 描画時にまとめて処理されるため描画コール数が削減され、パフォーマンスが向上します。

### 設計

- バッチキーは `std::uint64_t` 型の値です。
- **同じバッチキー + 同じパイプライン** を持つオブジェクト同士がバッチングの対象になります。
- デフォルトのバッチキーはオブジェクト名のハッシュ値です。同じ名前のオブジェクトは自動的にバッチングされます。
- バッチキーに加えて `RenderType` を指定できます。
  - `RenderType::Instancing`（デフォルト）：インスタンシング描画でまとめられます。
  - `RenderType::Standard`：個別描画になります（バッチングされません）。

### 公開API

```cpp
// バッチキーを明示的に設定（同じキーのオブジェクト同士がバッチングされる）
void SetBatchKey(std::uint64_t key, RenderType renderType = RenderType::Instancing);

// バッチキーを一意な値に設定（他のオブジェクトとバッチングされなくなる）
void SetUniqueBatchKey();

// 現在のバッチキーを取得
std::uint64_t GetInstanceBatchKey() const;
```

### 使い分けの指針

- **同じ見た目のオブジェクトを大量に描画する場合（敵、弾、パーティクルなど）**
  - 同じバッチキーを設定 → インスタンシングで効率よく描画されます。
- **個別にシェーダー変数を切り替えたいオブジェクト（UI 要素など）**
  - `SetUniqueBatchKey()` で個別描画にします。
- **同じパイプラインの ScreenBuffer 表示用スプライトなど**
  - テクスチャが異なるため、`SetUniqueBatchKey()` で分離する必要があります。

### 例：同じバッチキーでインスタンシング描画

```cpp
// 同じバッチキーを設定すると、同一パイプライン上でまとめて描画される
constexpr std::uint64_t kEnemyBatchKey = 100;

auto enemy1 = std::make_unique<KashipanEngine::Box>();
enemy1->SetBatchKey(kEnemyBatchKey);
enemy1->AttachToRenderer(screenBuffer, "Object3D.Solid.BlendNormal");

auto enemy2 = std::make_unique<KashipanEngine::Box>();
enemy2->SetBatchKey(kEnemyBatchKey);
enemy2->AttachToRenderer(screenBuffer, "Object3D.Solid.BlendNormal");

// enemy1 と enemy2 は 1 回の描画コールでまとめて描画される
```

### 例：個別描画に設定

```cpp
auto sprite = std::make_unique<KashipanEngine::Sprite>();
sprite->SetUniqueBatchKey(); // 他のオブジェクトとバッチングしない
sprite->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
```

> `SetUniqueBatchKey()` はオブジェクト名とポインタのハッシュを組み合わせた一意なキーを内部的に生成し、`RenderType::Standard` を設定します。

---

## 例：最小の 2D オブジェクト

`Object2DBase` は派生クラス側で `OnUpdate()` をオーバーライドして挙動を書けます。

```cpp
#include <KashipanEngine.h>

class MyObject2D final : public KashipanEngine::Object2DBase {
public:
    MyObject2D() : Object2DBase("MyObject2D") {
        // 例：必要なコンポーネントを登録
        // RegisterComponent<MyComponent2D>(...);
    }

private:
    void OnUpdate() override {
        // 毎フレーム処理
    }
};
```

---

## 例：永続レンダーパス登録（Window へ描画）

描画は `Renderer` に「永続レンダーパス」を登録する方式になっています。

```cpp
auto h = obj->AttachToRenderer(
    Window::GetWindow("Main Window"),
    "Object2D.Solid.BlendNormal" // 例：パイプライン名（json/asset 側と一致させる）
);

// 解除
obj->DetachFromRenderer(h);
```

> パイプライン名はプロジェクトのパイプライン定義（例：`Assets/.../Pipelines/*.json`）と対応します。

---

## 例：ユーザーデータ領域の利用

`Object2DBase::UserData()` はアプリ側の一時データ管理向けです。

```cpp
obj->UserData()["Score"].SetValue<int>(100);

int score = 0;
obj->UserData().at("Score").TryGetValue(score);
```

※ `AnyUnorderedMap` / `Any` の操作は `MyStd` 側の API に従います。
