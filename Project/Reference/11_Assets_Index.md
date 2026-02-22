# アセット管理（カテゴリ別インデックス）

エンジン側で用意しているアセット管理クラスは `KashipanEngine/AssetsHeaders.h` で一括インクルードできます。

```cpp
#include <KashipanEngine.h>
// もしくは
#include "KashipanEngine/AssetsHeaders.h"
```

この章はアセット管理をカテゴリごとに分割し、各カテゴリの目的・設計・具体的な使い方を記述します。

---

## カテゴリ一覧

- `11_Assets_TextureManager.md`：テクスチャ管理（`TextureManager` / `TextureView` / `IShaderTexture`）
- `11_Assets_ModelManager.md`：モデル管理（`ModelManager` / `ModelData`）
- `11_Assets_AudioManager.md`：音声管理（`AudioManager`）
- `11_Assets_AudioPlayer.md`：音声プレイヤー（`AudioPlayer`）
- `11_Assets_SoundBeat.md`：ビート検出（`SoundBeat`）
- `11_Assets_SamplerManager.md`：サンプラー管理（`SamplerManager` / `DefaultSampler`）

---

## 共通設計

### `Assets` 配下のファイルは「起動時に読み込まれる」

各マネージャ（`TextureManager` / `ModelManager` / `AudioManager`）は内部に `LoadAllFromAssetsFolder()` を持ち、
**Assets ルート以下を起動時に走査して事前ロード**する設計になっています。

そのため、基本運用は以下です。

- ゲームで使うファイルを `Assets/` 配下へ配置しておく
- 実行時は「ファイル名」または「Assets ルートからの相対パス」でハンドルを取得して使う

> 事前ロードの対象拡張子や探索ルールは各 `*Manager.cpp` の実装に従います。

### `SceneBase`（エンジン資産への静的アクセサ）

- `static AudioManager* GetAudioManager()`
- `static ModelManager* GetModelManager()`
- `static SamplerManager* GetSamplerManager()`
- `static TextureManager* GetTextureManager()`

これらのポインタは `GameEngine` が `SceneBase::SetEnginePointers(...)` で設定します。

### `Assets` の配置（実務上のルール）

- `Assets/` 直下〜配下に配置したファイルがロード対象
- engine 付属のデータ（パイプライン定義、エンジンロゴ等）は `Assets/KashipanEngine/...` に配置されている想定

例：
- `Assets/KashipanEngine/EngineSettings.json`
- `Assets/KashipanEngine/Pipeline/...`（パイプライン定義やプリセット）

> `EngineSettings.json` の内容は `GameEngine` 起動時に読み込まれ、ウィンドウ初期パラメータ等に反映されます。

### ファイル名指定時について

> ファイル名指定時の文字の大文字小文字は、Windowsの仕様に則って関係しないようにしています。

内部ではファイル名・アセットパスの検索に大文字小文字を区別しない `FileMap`（`CaseInsensitiveHash` / `CaseInsensitiveEqual`）を使用しています。

---

## 旧ページについて

従来の `11_Assets.md` は「全アセットマネージャの単一ページ列挙」でしたが、
説明の厳密化と使用例の拡充のためカテゴリ別ページへ分割しました。

- 旧：`11_Assets.md`
- 新：本インデックス＋カテゴリ別ページ群
