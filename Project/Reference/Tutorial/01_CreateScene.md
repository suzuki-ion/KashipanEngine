# 1. シーンの作成

KashipanEngine のシーンは `SceneBase` を継承して作成します。

- ヘッダ：`KashipanEngine/Scene/SceneBase.h`
- 代表的なオーバーライド：
  - `Initialize()`：シーン開始時（SceneManager から呼ばれる）
  - `Finalize()`：シーン終了時（SceneManager から呼ばれる）
  - `OnUpdate()`：毎フレーム更新（`SceneBase::Update()` から呼ばれる）

---

## 最小構成の例

```cpp
#include <KashipanEngine.h>

namespace KashipanEngine {

class MyScene final : public SceneBase {
public:
    MyScene() : SceneBase("MyScene") {}
    ~MyScene() override = default;

    void Initialize() override {
        // シーン開始時の一度だけ行う処理
    }

    void Finalize() override {
        // シーン終了時の後片付け
    }

private:
    void OnUpdate() override {
        // 毎フレーム処理
        // 例：入力を見るなら GetInputCommand() 等を利用
    }
};

} // namespace KashipanEngine
```

---

## ポイント（厳密）

- `SceneBase` はデフォルトコンストラクタが削除されているため、必ず `SceneBase("SceneName")` で構築します。
- `SceneBase` は `GetInput()` / `GetInputCommand()` / 各種 AssetManager へのアクセサを **protected static** に持ちます。
  - シーン派生クラス内部では `GetInputCommand()` 等を直接呼べます。
