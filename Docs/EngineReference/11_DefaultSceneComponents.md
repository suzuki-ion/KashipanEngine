# デフォルトで用意されているシーンコンポーネント

対象コード:

- `KashipanEngine/SceneHeaders.h`
- `KashipanEngine/Scene/Components/*`

## 1. 一覧（SceneHeaders.h 基準）

- `ColliderComponent`

## 2. ColliderComponent

ファイル: `KashipanEngine/Scene/Components/ColliderComponent.h`

- `ISceneComponent("ColliderComponent")` として定義
- 内部に `Collider collider_{}` を保持

主な挙動:

- `Update()`
  - `collider_.Update2D();`
  - `collider_.Update3D();`
- `Finalize()`
  - `collider_.Clear2D();`
  - `collider_.Clear3D();`

`GetCollider()` で `Collider*` を取得できます。
