# シーン（`SceneBase` / `SceneManager`）

シーンは「ゲームの状態（Title / Game / Result 等）」を表す単位で、`SceneBase` を継承して作ります。
シーンの生成・切替は `SceneManager` が担当します。

---

## 公開API：`SceneManager`（`KashipanEngine/Scene/SceneManager.h`）

- `template<typename TScene, typename... Args> void RegisterScene(const std::string& sceneName, Args&&... args)`
  - `TScene` は `SceneBase` 派生である必要があります。
  - `args...` はシーン生成時に `TScene` のコンストラクタへ転送されます。
- `SceneBase* GetCurrentScene() const`
- `bool ChangeScene(const std::string& sceneName)`
  - 即時に切り替えず **保留**をセットします。
- `void CommitPendingSceneChange(Passkey<GameEngine>)`
  - `GameEngine` がフレーム終端で呼び出して保留を反映します。
- シーン変数
  - `void AddSceneVariable(const std::string& key, const std::any& value)`
  - `const MyStd::AnyUnorderedMap& GetSceneVariables()`

---

## 公開API：`SceneBase`（`KashipanEngine/Scene/SceneBase.h`）

- ライフサイクル
  - `virtual void Initialize()`（`SceneManager` から呼ばれる）
  - `virtual void Finalize()`（`SceneManager` から呼ばれる）
  - `void Update()`（内部で `OnUpdate()` / オブジェクト更新 / コンポーネント更新等を行う）
- オーバーライドポイント
  - `virtual void OnUpdate()`
- オブジェクト管理
  - `bool AddObject2D(std::unique_ptr<Object2DBase> obj)`
  - `bool AddObject3D(std::unique_ptr<Object3DBase> obj)`
  - `bool RemoveObject2D(Object2DBase* obj)` / `bool RemoveObject3D(Object3DBase* obj)`
  - `void ClearObjects2D()` / `void ClearObjects3D()`
- シーンコンポーネント
  - `bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp)`
  - `bool RemoveSceneComponent(ISceneComponent* comp)`
  - `void ClearSceneComponents()`
  - `template<typename T> T* GetSceneComponent() const`
  - `std::vector<ISceneComponent*> GetSceneComponents(const std::string& componentName) const`
- 次シーン遷移
  - `void SetNextSceneName(const std::string&)`
  - `void ChangeToNextScene()`
  - `void ClearNextSceneName()`
- シーン変数読み取り
  - `template<typename T> bool TryGetSceneVariable(const std::string& key, T& out) const`
  - `template<typename T> T GetSceneVariableOr(const std::string& key, const T& defaultValue) const`
- エンジン資産へのアクセス（静的）
  - `static AudioManager* GetAudioManager()`
  - `static ModelManager* GetModelManager()`
  - `static SamplerManager* GetSamplerManager()`
  - `static TextureManager* GetTextureManager()`
  - `static Input* GetInput()`
  - `static InputCommand* GetInputCommand()`

---

## 例：シーン最小スケルトン

```cpp
#include <KashipanEngine.h>

class MyScene final : public KashipanEngine::SceneBase {
public:
    MyScene() : SceneBase("MyScene") {}

    void Initialize() override {
        // オブジェクト/シーンコンポーネント追加
    }

    void Finalize() override {
        // 必要なら解放
    }

private:
    void OnUpdate() override {
        // 毎フレーム処理
    }
};
```

---

## 例：シーン切替（次シーン名をセットして遷移）

```cpp
void MyScene::OnUpdate() {
    // 例：条件を満たしたら
    SetNextSceneName("TitleScene");
    ChangeToNextScene();
}
```

---

## 例：シーン変数を読む

```cpp
void MyScene::OnUpdate() {
    int difficulty = GetSceneVariableOr<int>("Difficulty", 0);
    (void)difficulty;
}
```
