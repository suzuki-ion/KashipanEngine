# 8. モデルのハンドルを取得し、モデルオブジェクトを作成したいとき

モデルは `ModelManager` が管理し、`ModelHandle`（`uint32_t`）で識別します。

- ヘッダ：`KashipanEngine/Assets/ModelManager.h`

`Model` オブジェクトは `ModelData`（頂点/インデックス/マテリアル等）を元に生成します。

- ヘッダ：`KashipanEngine/Objects/GameObjects/3D/Model.h`

---

## 8.1 `ModelData` を取得する

ファイル名から `ModelData` を取得できます。

```cpp
const auto& data = KashipanEngine::ModelManager::GetModelDataFromFileName("player.obj");
```

相対パスから取得したい場合：

```cpp
const auto& data = KashipanEngine::ModelManager::GetModelDataFromAssetPath("Models/player.obj");
```

---

## 8.2 `Model` オブジェクトを作成する

```cpp
#include <KashipanEngine.h>

const auto& data = KashipanEngine::ModelManager::GetModelDataFromFileName("player.obj");
auto obj = std::make_unique<KashipanEngine::Model>(data);
obj->SetName("PlayerRoot");
```

---

## 8.3 描画先に登録する

```cpp
if (auto* sb = screenBuffer3D) {
    obj->AttachToRenderer(sb, "Object3D.Solid.BlendNormal");
}
```

---

## 注意点（厳密）

- `GetModelDataFromFileName` はファイル名単体で検索します。
  - 同名ファイルが複数ある場合、どれが返るかは `ModelManager` 実装に依存します。
- 取得に失敗した場合は「空データ（empty data）」が返る設計になっています（`ModelManager` ヘッダに `sEmptyData` が存在）。
