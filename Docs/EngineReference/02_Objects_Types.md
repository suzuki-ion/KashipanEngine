# `KashipanEngine/Objects` オブジェクト種類リファレンス

このドキュメントは `KashipanEngine/ObjectsHeaders.h` の include 一覧（＝エンジンが標準で公開しているオブジェクト群）を基準に整理しています。

## 1. 基底クラス

- `Object2DBase`
  - 2D オブジェクトの基底。
  - `Update()` で登録済み 2D コンポーネントを更新。
  - `AttachToRenderer(Window*, pipelineName)` / `AttachToRenderer(ScreenBuffer*, pipelineName)` で永続描画パスを登録。
- `Object3DBase`
  - 3D オブジェクトの基底。
  - `AttachToRenderer(Window*)` / `AttachToRenderer(ScreenBuffer*)` / `AttachToRenderer(ShadowMapBuffer*)` を持つ（ShadowMap は 3D のみ）。
- `ObjectContext` (`Object2DContext` / `Object3DContext`)
  - オブジェクトからコンポーネントを取得するためのコンテキスト API も提供（詳細は別ドキュメント）。

## 2. GameObjects（描画用オブジェクト）

### 2D (`Objects/GameObjects/2D`)

- `Triangle2D`
- `Rect`
- `Ellipse`
- `Sprite`
- `Line2D`
- `VertexData2D`（頂点構造体）

### 3D (`Objects/GameObjects/3D`)

- `Triangle3D`
- `Sphere`
- `Box`
- `Plane3D`
- `Line3D`
- `Billboard`
- `Model`
  - `Model(const std::string &relativePath)` のように Assets 相対パスから生成可能。
  - `ModelManager` を内部で使用（`Model::SetModelManager(Passkey<GameEngine>, ModelManager*)` が存在）。
- `VertexData3D`（頂点構造体）
- `FaceNormal3D`（法線可視化などに利用される想定の型）

## 3. SystemObjects（システム寄りオブジェクト）

- `Camera2D`
- `Camera3D`
  - `Perspective` / `Orthographic` を切り替え可能。
  - `GetViewMatrix()`, `GetProjectionMatrix()`, `GetViewProjectionMatrix()` 等。
  - `GetCameraBufferCPU()` で CPU 側のカメラ定数構造体を取得。
- `DirectionalLight`
- `ShadowMapBinder`

## 4. Components（オブジェクトコンポーネント）

2D/3D の標準コンポーネント（詳細は別ドキュメント）:

- 2D: `Transform2D`, `Material2D`, `Collision2D`
- 3D: `Transform3D`, `Material3D`, `Collision3D`

## 5. Collision / MathObjects

- `Objects/Collision`
  - `Collider`
  - `CollisionAlgorithms2D`
  - `CollisionAlgorithms3D`
- `Objects/MathObjects`
  - 2D/3D の当たり判定・図形表現用の型（例: `Circle`, `Ray`, `AABB`, `OBB`, `Plane` 等）

> 実際に含まれる全ファイルは `KashipanEngine/Objects/MathObjects` 配下を参照してください（本ドキュメントはカテゴリの説明を主目的にしています）。
