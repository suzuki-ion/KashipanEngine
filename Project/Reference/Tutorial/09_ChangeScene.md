# 9. 次のシーンに遷移したいとき

シーン遷移は、`SceneBase` が持つ
- `SetNextSceneName(...)`
- `ChangeToNextScene()`

を使用して行います。

- ヘッダ：`KashipanEngine/Scene/SceneBase.h`

---

## 9.1 基本：次のシーン名をセットして切り替える

`SceneBase` 派生クラスの中から、条件を満たしたフレームで次を設定し、切り替えます。

```cpp
#include <KashipanEngine.h>

void MyScene::OnUpdate() {
    if (GetInputCommand() && GetInputCommand()->Evaluate("Submit").Triggered()) {
        if (GetNextSceneName().empty()) {
            SetNextSceneName("GameScene");
        }
        ChangeToNextScene();
    }
}
```

---

## 9.2 既存実装の例：演出完了後に遷移

`TitleScene` の `OnUpdate()` では、
- 入力 or アニメ終了で `SetNextSceneName(...)`
- `SceneChangeOut` などの演出コンポーネントが完了したら `ChangeToNextScene()`

という流れになっています。

- 参照：`Application/Scenes/TitleScene.cpp`

この方式により、単純即時遷移ではなく
- フェード
- ワイプ

などの演出を「シーンコンポーネント」として挟めます。

---

## 9.3 厳密な注意点

- `ChangeToNextScene()` は、`SetNextSceneName(...)` で次がセットされているときにのみ意味を持ちます。
- 実際の切り替えタイミング（即時か、フレーム終端コミットか）は `SceneManager` 実装に従います。
  - `GameEngine` のメインループでは、フレーム末尾で `sceneManager_->CommitPendingSceneChange(...)` が呼ばれます。
