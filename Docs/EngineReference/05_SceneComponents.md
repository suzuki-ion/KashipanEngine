# シーンコンポーネント

対象コード:

- `KashipanEngine/Scene/Components/ISceneComponent.h/.cpp`
- `KashipanEngine/Scene/SceneBase.h/.cpp`
- `KashipanEngine/Scene/SceneContext.h/.cpp`
- `KashipanEngine/SceneHeaders.h`

## 1. シーンコンポーネントの基本

シーンコンポーネントは `ISceneComponent` を継承して実装します。

`ISceneComponent` の主な要素:

- `Initialize()` / `Finalize()` / `Update()`
- `GetComponentType()`
  - コンストラクタで渡した文字列（`kComponentType_`）が返る
- `GetMaxComponentCountPerScene()`
  - 同一 type の登録上限
- `GetUpdatePriority()` / `SetUpdatePriority(priority)`
  - `SceneBase::Update()` 内で `GetUpdatePriority()` により安定ソートされ、低い方から更新されます
- `SetOwnerContext(SceneContext*)`
  - `SceneBase::AddSceneComponent` 時に自動で設定されます
- `GetOwnerContext()`
- `GetOwnerScene()`

## 2. シーンへの登録（使い方）

`SceneBase` の protected API:

- `AddSceneComponent(std::unique_ptr<ISceneComponent> comp)`
  - 上限チェック（`GetMaxComponentCountPerScene()`）
  - `comp->SetOwnerContext(sceneContext_.get())`
  - `comp->Initialize()`
  - `sceneComponents_` に保存
  - 型・名前のインデックスを構築

- `RemoveSceneComponent(ISceneComponent *comp)`
  - `Finalize()` を呼んでから削除
  - インデックス再構築

- `ClearSceneComponents()`
  - 全コンポーネントに `Finalize()` を呼んで破棄

## 3. シーンからコンポーネントを取得する方法

### A. `SceneBase` のメソッドで取得

- 名前で取得
  - `GetSceneComponent(componentName)`
  - `GetSceneComponents(componentName)`
- 型で取得
  - `GetSceneComponent<T>()`
  - `GetSceneComponents<T>()`

### B. `SceneContext` で取得

`SceneContext` は SceneComponent 側に渡される「シーン参照 API」です。

- `SceneContext::GetComponent(name)` / `GetComponents(name)`
- `SceneContext::GetComponent<T>()` / `GetComponents<T>()`

## 4. 標準で用意されているシーンコンポーネント

`KashipanEngine/SceneHeaders.h` で公開されている標準シーンコンポーネント:

- `ColliderComponent`
  - `Collider` を内部に保持し、毎フレーム `Update2D/Update3D` を呼ぶ
  - `Finalize()` で `Clear2D/Clear3D`

（現状、`KashipanEngine/Scene/Components` 配下は `ColliderComponent` のみです）
