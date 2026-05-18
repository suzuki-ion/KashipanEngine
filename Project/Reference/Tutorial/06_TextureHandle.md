# 6. テクスチャのハンドルを取得し、オブジェクトに適用したいとき

テクスチャは `TextureManager` が管理し、`TextureHandle`（`uint32_t`）で識別します。

- ヘッダ：`KashipanEngine/Assets/TextureManager.h`

---

## 6.1 ハンドル型と無効値

- `TextureManager::TextureHandle`：`uint32_t`
- 無効値：`TextureManager::kInvalidHandle`（0）

---

## 6.2 ハンドルの取得（既にロード済みの前提）

このプロジェクトでは Assets 起動時にテクスチャがロードされるため、通常は「取得」だけを行います。

- ファイル名単体から取得：

```cpp
KashipanEngine::TextureManager::TextureHandle tex =
    KashipanEngine::TextureManager::GetTextureFromFileName("white1x1.png");
```

- Assets ルート相対パスから取得：

```cpp
KashipanEngine::TextureManager::TextureHandle tex =
    KashipanEngine::TextureManager::GetTextureFromAssetPath("KashipanEngine/Textures/white1x1.png");
```

---

## 6.3 その場で読み込みたい場合（必要なときだけ）

`TextureManager::LoadTexture(filePath)` は、Assets ルート相対パスまたはフルパスを受け取ります。

```cpp
// ※ TextureManager インスタンスが必要（通常は GameEngine が所有）
// TextureManager* tm = ...;
// auto tex = tm->LoadTexture("Textures/new.png");
```

---

## 6.4 `Sprite` に適用する例（`Material2D`）

```cpp
#include <KashipanEngine.h>

auto obj = std::make_unique<KashipanEngine::Sprite>();
obj->SetName("MySprite");

auto tex = KashipanEngine::TextureManager::GetTextureFromFileName("white1x1.png");

if (auto* mat = obj->GetComponent2D<KashipanEngine::Material2D>()) {
    mat->SetTexture(tex);
    mat->SetColor(KashipanEngine::Vector4(1, 1, 1, 1));
}
```

---

## 6.5 `Model` に適用する例（`Material3D`）

```cpp
auto tex = KashipanEngine::TextureManager::GetTextureFromFileName("uvChecker.png");

if (auto* mat = obj3d->GetComponent3D<KashipanEngine::Material3D>()) {
    mat->SetTexture(tex);
}
```

---

## 注意点（厳密）

- `GetTextureFromFileName` は「ファイル名単体」で検索します。
  - 同名ファイルが複数ある場合、どれが返るかは `TextureManager` 実装に依存します。
- 取得/読み込みに失敗した場合は `TextureManager::kInvalidHandle`（0）が返ります。
- シェーダーバインドを行う場合は `TextureManager::BindTexture(...)` が利用できます。
