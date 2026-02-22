# アセット管理：テクスチャ（`TextureManager`）

対象ヘッダ：`KashipanEngine/Assets/TextureManager.h`

関連：`KashipanEngine/Graphics/IShaderTexture.h`

---

## 1. 目的

テクスチャの読み込み・管理・シェーダーへのバインドを一括して行うクラスです。
D3D12 の SRV ハンドルを外部へ直接公開せず、ハンドルベースの抽象化を提供します。

---

## 2. 設計概要

- テクスチャは `TextureHandle`（`uint32_t`）で管理します。
- 無効ハンドルは `TextureManager::kInvalidHandle`（`0`）です。
- `Assets/` 配下のファイルはエンジン起動時に `LoadAllFromAssetsFolder()` で自動ロードされます。
- ファイル名・アセットパスでの検索は大文字小文字を区別しません。

### 対応画像形式

`.png` / `.jpg` / `.jpeg` / `.bmp` / `.tga` / `.dds` / `.hdr` / `.tif` / `.tiff` / `.gif` / `.webp`

---

## 3. 公開API

### 3.1 型定義・定数

```cpp
using TextureHandle = uint32_t;
static constexpr TextureHandle kInvalidHandle = 0;
```

### 3.2 コンストラクタ（`GameEngine` からのみ生成可能）

```cpp
TextureManager(Passkey<GameEngine>, DirectXCommon* directXCommon, const std::string& assetsRootPath = "Assets");
```

### 3.3 読み込み

```cpp
TextureHandle LoadTexture(const std::string& filePath);
```

- `Assets` ルート相対パス or フルパスでテクスチャをロードします。
- 既にロード済みのパスが指定された場合は既存のハンドルを返します。
- 失敗時は `kInvalidHandle` を返します。

### 3.4 取得

```cpp
static TextureHandle GetTexture(TextureHandle handle);
static TextureHandle GetTextureFromFileName(const std::string& fileName);
static TextureHandle GetTextureFromAssetPath(const std::string& assetPath);
```

- `GetTexture`: ハンドルが有効なら同じハンドルを返します（存在チェック用途）。
- `GetTextureFromFileName`: ファイル名単体から検索します。
- `GetTextureFromAssetPath`: Assets ルートからの相対パスから検索します。

### 3.5 ビュー取得

```cpp
static TextureView GetTextureView(TextureHandle handle);
```

- `IShaderTexture` インターフェースとして扱えるビューオブジェクトを返します。
- 無効ハンドルの場合は空のビュー（SRV ハンドル = `0`）を返します。

### 3.6 バインド

```cpp
static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, TextureHandle handle);
static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, const IShaderTexture& texture);
```

- シェーダー変数バインダー経由でテクスチャをシェーダーにバインドします。
- `nameKey` はシェーダーステージとスロット名の組み合わせ（例：`"Pixel:gDiffuseTexture"`）を指定します。

---

## 4. `TextureView`（ネストクラス）

```cpp
class TextureView final : public IShaderTexture {
public:
    TextureView() = default;
    explicit TextureView(TextureHandle h);

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;
    std::uint32_t GetWidth() const noexcept override;
    std::uint32_t GetHeight() const noexcept override;

    TextureHandle GetHandle() const noexcept;
};
```

- `TextureManager::GetTextureView(handle)` から取得します。
- `IShaderTexture` を実装しているため、`BindTexture` の `IShaderTexture` オーバーロードや、テクスチャを受け取る他のAPIに渡せます。

---

## 5. `IShaderTexture`（テクスチャ共通インターフェース）

ヘッダ：`KashipanEngine/Graphics/IShaderTexture.h`

```cpp
class IShaderTexture {
public:
    virtual D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept = 0;
    virtual std::uint32_t GetWidth() const noexcept = 0;
    virtual std::uint32_t GetHeight() const noexcept = 0;
};
```

- `TextureView` や `ScreenBuffer` の描画結果など、SRV を持つオブジェクトを統一的に扱うためのインターフェースです。

---

## 6. 使用例

### ファイル名からテクスチャを取得してバインド

```cpp
auto tex = KashipanEngine::TextureManager::GetTextureFromFileName("player.png");

// binder は Object の Render() / BindShaderVariables() 等から渡される想定
KashipanEngine::TextureManager::BindTexture(&binder, "Pixel:gDiffuseTexture", tex);
```

### TextureView を IShaderTexture として扱う

```cpp
auto view = KashipanEngine::TextureManager::GetTextureView(tex);
KashipanEngine::TextureManager::BindTexture(&binder, "Pixel:gDiffuseTexture", view);
```

### Assets パスから取得

```cpp
auto tex = KashipanEngine::TextureManager::GetTextureFromAssetPath("KashipanEngine/Logo.png");
```
