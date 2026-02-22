# アセット管理：モデル（`ModelManager`）

対象ヘッダ：`KashipanEngine/Assets/ModelManager.h`

---

## 1. 目的

3Dモデルファイルの読み込み・管理を行うクラスです。
Assimp ライブラリを使用して多様なモデル形式をサポートし、頂点・インデックス・マテリアル情報を `ModelData` として保持します。

---

## 2. 設計概要

- モデルは `ModelHandle`（`uint32_t`）で管理します。
- 無効ハンドルは `ModelManager::kInvalidHandle`（`0`）です。
- `Assets/` 配下のファイルはエンジン起動時に `LoadAllFromAssetsFolder()` で自動ロードされます。
- ファイル名・アセットパスでの検索は大文字小文字を区別しません。

### 対応モデル形式

`.fbx` / `.obj` / `.gltf` / `.glb` / `.dae` / `.3ds` / `.blend` / `.ply` / `.stl` / `.x`

---

## 3. 公開API

### 3.1 型定義・定数

```cpp
using ModelHandle = uint32_t;
static constexpr ModelHandle kInvalidHandle = 0;
```

### 3.2 コンストラクタ（`GameEngine` からのみ生成可能）

```cpp
ModelManager(Passkey<GameEngine>, const std::string& assetsRootPath = "Assets");
```

### 3.3 読み込み

```cpp
ModelHandle LoadModel(const std::string& filePath);
```

- `Assets` ルート相対パス or フルパスでモデルをロードします。
- 既にロード済みのパスが指定された場合は既存のハンドルを返します。
- 失敗時は `kInvalidHandle` を返します。

### 3.4 ハンドル取得

```cpp
static ModelHandle GetModelHandleFromFileName(const std::string& fileName);
static ModelHandle GetModelHandleFromAssetPath(const std::string& assetPath);
```

- `GetModelHandleFromFileName`: ファイル名単体から検索します。
- `GetModelHandleFromAssetPath`: Assets ルートからの相対パスから検索します。

### 3.5 モデルデータ取得

```cpp
static const ModelData& GetModelData(ModelHandle handle);
static const ModelData& GetModelDataFromFileName(const std::string& fileName);
static const ModelData& GetModelDataFromAssetPath(const std::string& assetPath);
```

- ハンドル、ファイル名、アセットパスのいずれかから `ModelData` の const 参照を返します。
- 見つからない場合は空の `ModelData`（頂点数・インデックス数 0）を返します。

---

## 4. `ModelData`（モデルデータ構造体）

```cpp
struct ModelData final {
    // public
    uint32_t GetVertexCount() const noexcept;
    uint32_t GetIndexCount() const noexcept;
    const std::string& GetAssetRelativePath() const noexcept;
    uint32_t GetMaterialCount() const noexcept;
    const MaterialData* GetMaterial(uint32_t idx) const noexcept;
};
```

- `ModelData` は `ModelManager` と `Model` の `friend` としてのみ構築可能です（コンストラクタは private）。
- 内部に頂点データ（`Vertex`）、インデックスデータ（`uint32_t`）、マテリアルデータ（`MaterialData`）を保持します。

### 4.1 `ModelData::Vertex`

```cpp
struct Vertex final {
    float px, py, pz;   // 位置
    float nx, ny, nz;   // 法線
    float u, v;          // テクスチャ座標（UV0）
};
```

### 4.2 `ModelData::MaterialData`

```cpp
struct MaterialData final {
    float baseColor[4];               // ベースカラー（RGBA、デフォルトは白）
    std::string diffuseTexturePath;   // ディフューズテクスチャの Assets 相対パス
};
```

- `baseColor` はモデルファイルから取得した Diffuse カラーです。
- `diffuseTexturePath` はテクスチャが存在する場合、Assets ルートからの相対パスが格納されます。

### 4.3 公開メソッド一覧

- `GetVertexCount()` - 頂点数を返します。
- `GetIndexCount()` - インデックス数を返します。
- `GetAssetRelativePath()` - Assets ルートからの相対パスを返します。
- `GetMaterialCount()` - マテリアル数を返します。
- `GetMaterial(idx)` - 指定インデックスのマテリアルを返します（範囲外は `nullptr`）。

---

## 5. 読み込み処理の詳細

モデル読み込み時には Assimp に以下のポストプロセスフラグを適用しています。

- `aiProcess_MakeLeftHanded` - 左手座標系への変換
- `aiProcess_FlipWindingOrder` - ワインディング順の反転
- `aiProcess_Triangulate` - ポリゴンの三角形化
- `aiProcess_JoinIdenticalVertices` - 同一頂点の結合
- `aiProcess_ImproveCacheLocality` - キャッシュ効率の改善
- `aiProcess_RemoveRedundantMaterials` - 冗長マテリアルの除去
- `aiProcess_FindInvalidData` - 不正データの検出
- `aiProcess_TransformUVCoords` - UV座標の変換
- `aiProcess_SortByPType` - プリミティブタイプでのソート
- `aiProcess_FlipUVs` - UV座標のY軸反転

---

## 6. 使用例

### ファイル名からモデルデータを参照

```cpp
auto h = KashipanEngine::ModelManager::GetModelHandleFromFileName("enemy.fbx");
const auto& data = KashipanEngine::ModelManager::GetModelData(h);

uint32_t vtx = data.GetVertexCount();
uint32_t idx = data.GetIndexCount();
(void)vtx;
(void)idx;
```

### マテリアル情報を参照

```cpp
const auto& data = KashipanEngine::ModelManager::GetModelDataFromFileName("enemy.fbx");
for (uint32_t i = 0; i < data.GetMaterialCount(); ++i) {
    const auto* mat = data.GetMaterial(i);
    if (!mat) continue;
    // mat->baseColor[0..3] でベースカラー参照
    // mat->diffuseTexturePath でテクスチャパス参照
}
```

### Assets パスから直接モデルデータを取得

```cpp
const auto& data = KashipanEngine::ModelManager::GetModelDataFromAssetPath("Models/player.fbx");
```
