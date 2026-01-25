# オブジェクトコンポーネント

対象コード:

- `KashipanEngine/Objects/IObjectComponent.h`
- `KashipanEngine/Objects/Object2DBase.h/.cpp`
- `KashipanEngine/Objects/Object3DBase.h/.cpp`
- `KashipanEngine/Objects/ObjectContext.h/.cpp`

## 1. コンポーネントの基本

オブジェクトコンポーネントは `IObjectComponent` を基底とし、2D/3D それぞれ

- `IObjectComponent2D`
- `IObjectComponent3D`

を継承して実装します。

`IObjectComponent` の主な要素:

- `Initialize()` / `Finalize()` / `Update()`
  - 戻り値は `std::optional<bool>`
  - `std::nullopt`: 何もしない
  - `true/false`: 成功/失敗
- `BindShaderVariables(ShaderVariableBinder*)`
  - 返り値が `std::nullopt` 以外のコンポーネントは「シェーダ変数をバインドする予定がある」とみなされ、描画時に呼ばれる
- `Clone()`
  - 派生側で実装必須（`std::unique_ptr<IObjectComponent>` を返す）
- `SetOwnerContext(IObjectContext*)`
  - 登録時にオーナーオブジェクトのコンテキストが渡される

## 2. コンポーネントの作成（実装）

### 2D コンポーネント例（骨格）

- `class MyComp : public IObjectComponent2D`
  - コンストラクタで `IObjectComponent("MyComp", maxCount)` を呼ぶ
  - `ShowImGui()` は `USE_IMGUI` 時に必須
  - `Clone()` は自分自身のコピーを作って返す

### 3D コンポーネント例（骨格）

- `class MyComp : public IObjectComponent3D`
  - 2D と同様

## 3. オブジェクトへの登録（使い方）

`Object2DBase` / `Object3DBase` には以下の 2 種類の登録 API があります。

- テンプレート登録
  - `RegisterComponent<T>(args...)`
  - `T` は 2D オブジェクトなら `IObjectComponent2D`、3D なら `IObjectComponent3D` 派生である必要があります。
- 既存インスタンス登録
  - `RegisterComponent(std::unique_ptr<IObjectComponent> comp)`

登録時の重要点（`Object2DBase::RegisterComponent` / `Object3DBase::RegisterComponent`）:

- `dynamic_cast` で 2D/3D 不一致のコンポーネントは登録拒否
- 同名コンポーネント数が `GetMaxComponentCountPerObject()` を超えると登録拒否
- 登録時に `SetOwnerContext(...)` が呼ばれる
- その後 `Initialize()` が呼ばれる

## 4. オブジェクトからコンポーネントを取得する方法

取得方法は大きく 2 系統あります。

### A. `Object2DBase` / `Object3DBase` のメソッドで取得

`Object2DBase`（例）:

- 名前で取得
  - `GetComponent2D(const std::string &componentName)`
  - `GetComponents2D(const std::string &componentName)`
- 型で取得
  - `GetComponent2D<T>()`
  - `GetComponents2D<T>()`

3D も同様に `GetComponent3D...` があります。

### B. `ObjectContext`（`Object2DContext` / `Object3DContext`）で取得

`ObjectContext` は「オーナーオブジェクトに対する参照 API」です。

- `Object2DContext::GetComponent<T>()` / `GetComponents<T>()`
- `Object3DContext::GetComponent<T>()` / `GetComponents<T>()`
- 名前指定版もあります（`GetComponent(name)` 等）

コンポーネント実装側からは `IObjectComponent2D::GetOwner2DContext()` / `IObjectComponent3D::GetOwner3DContext()` 経由でアクセスできます。

## 5. 登録済み標準コンポーネント

エンジンの標準コンポーネントは `ObjectsHeaders.h` が公開しています。

- 2D: `Transform2D`, `Material2D`, `Collision2D`
- 3D: `Transform3D`, `Material3D`, `Collision3D`

また `Object2DBase` / `Object3DBase` はコンストラクタ内で `Transform` を自動登録しています。

- `Object2DBase::Object2DBase(const std::string&)` → `RegisterComponent<Transform2D>()`
- `Object3DBase::Object3DBase(const std::string&)` → `RegisterComponent<Transform3D>()`

（ジオメトリを持つコンストラクタでは `Material2D/3D` も登録されます）
