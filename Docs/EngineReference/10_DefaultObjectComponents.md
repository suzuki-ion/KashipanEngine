# デフォルトで用意されているオブジェクトコンポーネント

対象コード:

- `KashipanEngine/ObjectsHeaders.h`
- `KashipanEngine/Objects/Components/2D/*`
- `KashipanEngine/Objects/Components/3D/*`
- `KashipanEngine/Objects/Object2DBase.cpp` / `Object3DBase.cpp`（自動登録）

## 1. 一覧（ObjectsHeaders.h 基準）

### 2D

- `Transform2D`
- `Material2D`
- `Collision2D`

### 3D

- `Transform3D`
- `Material3D`
- `Collision3D`

## 2. 作成と自動登録

`Object2DBase` / `Object3DBase` は、少なくとも `Transform` を自動登録します。

- `Object2DBase::Object2DBase(const std::string&)`
  - `RegisterComponent<Transform2D>();`
- `Object3DBase::Object3DBase(const std::string&)`
  - `RegisterComponent<Transform3D>();`

また、ジオメトリ（頂点/インデックス）を持つコンストラクタでは `Material2D/3D` を登録します。

- `Object2DBase(... vertex/index ...)` で `Material2D` を登録
- `Object3DBase(... vertex/index ...)` で `Material3D` を登録

デフォルト設定:

- 既定テクスチャ: `TextureManager::GetTextureFromFileName("uvChecker.png")`
- 既定 sampler: `SamplerManager::SamplerHandle defaultSampler = 1;`

## 3. 各コンポーネントの役割（概要）

※ 詳細なフィールドや API は各ヘッダを参照してください。

- `Transform2D` / `Transform3D`
  - 位置・回転・スケール等の変換情報を扱い、インスタンシング用データ（`InstanceData`）を送る用途を持つ
- `Material2D` / `Material3D`
  - 色・テクスチャ・サンプラ等のマテリアル情報を扱い、インスタンシング用データ（`InstanceData`）を送る用途を持つ
- `Collision2D` / `Collision3D`
  - オブジェクト側の当たり判定情報/登録を扱う用途

> コンポーネントのシェーダ変数バインドは `IObjectComponent::BindShaderVariables` を使います。
> `BindShaderVariables(nullptr)` が `std::nullopt` 以外を返すコンポーネントは描画中にバインド対象として記録されます（`Object2DBase/Object3DBase` 実装）。
