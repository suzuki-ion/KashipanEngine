# シーン用コンポーネント（`ISceneComponent`）

シーン全体に 1 つ（または少数）だけ存在する機能は、`ISceneComponent` としてシーンに追加します。

例：
- デフォルト変数群（カメラ/ライト/ScreenBuffer など）
- 画面比率維持
- シャドウマップ同期
- シーン全体の進行（UI/フェーズ管理）

---

## 公開API：`ISceneComponent`（`KashipanEngine/Scene/Components/ISceneComponent.h`）

- 基本情報
  - `const std::string& GetComponentType() const`
  - `size_t GetMaxComponentCountPerScene() const`
- ライフサイクル
  - `virtual void Initialize()`
  - `virtual void Update()`
  - `virtual void Finalize()`
- 更新優先度
  - `int GetUpdatePriority() const`
  - `void SetUpdatePriority(int priority)`
- 所属コンテキスト
  - `void SetOwnerContext(SceneContext* context)`（エンジンが登録時に設定）
  - `SceneContext* GetOwnerContext() const`（protected）
  - `SceneBase* GetOwnerScene() const`（protected）

---

## 例：シーンコンポーネント実装

```cpp
#include <KashipanEngine.h>

class MySceneTicker final : public KashipanEngine::ISceneComponent {
public:
    MySceneTicker() : ISceneComponent("MySceneTicker", 1) {}

    void Initialize() override {
        // シーン初期化時に一度
    }

    void Update() override {
        // 毎フレーム
        auto* scene = GetOwnerScene();
        (void)scene;
    }

    void Finalize() override {
        // シーン終了時
    }
};
```

---

## 例：シーンへ追加・取得

```cpp
void MyScene::Initialize() {
    AddSceneComponent(std::make_unique<MySceneTicker>());

    auto* t = GetSceneComponent<MySceneTicker>();
    (void)t;
}
```

---

## `SceneDefaultVariables`（標準の便利コンポーネント）

`SceneDefaultVariables` は、シーン内でよく使う要素をまとめて提供します。

主な取得 API（`KashipanEngine/Scene/Components/SceneDefaultVariables.h`）：
- `ScreenBuffer* GetScreenBuffer2D() const` — 2D用ScreenBuffer（→ `07_Offscreen.md`）
- `ScreenBuffer* GetScreenBuffer3D() const` — 3D用ScreenBuffer（→ `07_Offscreen.md`）
- `Sprite* GetScreenBuffer2DSprite() const` — 2D用ScreenBufferスプライト（→ `02_GameObjects.md`）
- `Sprite* GetScreenBuffer3DSprite() const` — 3D用ScreenBufferスプライト（→ `02_GameObjects.md`）
- `Camera3D* GetMainCamera3D() const` — 3D用メインカメラ（→ `02_GameObjects.md`）
- `Camera2D* GetMainCamera2D() const` — 2D用メインカメラ（→ `02_GameObjects.md`）
- `ScreenBufferKeepRatio* GetKeepRatioComp() const` — ScreenBufferアスペクト比維持コンポーネント
- `ColliderComponent* GetColliderComp() const` — コライダーコンポーネント
- `DirectionalLight* GetDirectionalLight() const` — 平行光源（→ `08_Rendering.md`）
- `LightManager* GetLightManager() const` — ライト管理
- `ShadowMapBuffer* GetShadowMapBuffer() const` — シャドウマッピング用バッファ（→ `07_Offscreen.md`）
- `ShadowMapBinder* GetShadowMapBinder() const` — シャドウマッピング用バインダー
- `ShadowMapCameraSync* GetShadowMapCameraSync() const` — シャドウマッピング用カメラ同期
- `Camera3D* GetLightCamera3D() const` — シャドウマッピング用ライトカメラ（→ `02_GameObjects.md`）
- `Camera2D* GetWindowCamera2D() const` — ウィンドウ表示用2Dカメラ（→ `02_GameObjects.md`）
- `Window* GetMainWindow() const` — メインウィンドウ（→ `06_Window.md`）

利用例：
```cpp
void MyScene::Initialize() {
    AddSceneComponent(std::make_unique<KashipanEngine::SceneDefaultVariables>());
    auto* defaults = GetSceneComponent<KashipanEngine::SceneDefaultVariables>();
    if (!defaults) return;

    auto* mainWindow = defaults->GetMainWindow();
    auto* sb2d = defaults->GetScreenBuffer2D();
    (void)mainWindow;
    (void)sb2d;
}
