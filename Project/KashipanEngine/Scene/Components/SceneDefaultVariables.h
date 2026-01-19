#pragma once
#include "Core/Window.h"
#include "Scene/Components/ISceneComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/ScreenBufferKeepRatio.h"
#include "Scene/Components/ShadowMapCameraSync.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/SystemObjects/LightManager.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"
#include "Objects/GameObjects/2D/Sprite.h"

namespace KashipanEngine {

/// @brief シーン内で使用するデフォルト変数群
class SceneDefaultVariables : public ISceneComponent {
public:
    SceneDefaultVariables()
        : ISceneComponent("SceneDefaultVariables", 1) {}
    ~SceneDefaultVariables() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    /// @brief シーンコンポーネント設定
    /// @param registerFunc 登録用関数
    void SetSceneComponents(std::function<bool(std::unique_ptr<ISceneComponent>)> registerFunc);

    /// @brief 2D用ScreenBuffer取得
    ScreenBuffer *GetScreenBuffer2D() const { return screenBuffer2D_; }
    /// @brief 3D用ScreenBuffer取得
    ScreenBuffer *GetScreenBuffer3D() const { return screenBuffer3D_; }
    /// @brief 2D用ScreenBufferスプライト取得
    Sprite *GetScreenBuffer2DSprite() const { return screenBuffer2DSprite_; }
    /// @brief 3D用ScreenBufferスプライト取得
    Sprite *GetScreenBuffer3DSprite() const { return screenBuffer3DSprite_; }
    /// @brief 3D用メインカメラ取得
    Camera3D *GetMainCamera3D() const { return mainCamera3D_; }
    /// @brief 2D用メインカメラ取得
    Camera2D *GetMainCamera2D() const { return mainCamera2D_; }
    /// @brief ScreenBufferアスペクト比維持コンポーネント取得
    ScreenBufferKeepRatio *GetKeepRatioComp() const { return keepRatioComp_; }
    /// @brief コライダーコンポーネント取得
    ColliderComponent *GetColliderComp() const { return colliderComp_; }
    /// @brief 平行光源取得
    DirectionalLight *GetDirectionalLight() const { return directionalLight_; }
    /// @brief ライト管理用取得
    LightManager *GetLightManager() const { return lightManager_; }
    /// @brief シャドウマッピング用バッファ取得
    ShadowMapBuffer *GetShadowMapBuffer() const { return shadowMapBuffer_; }
    /// @brief シャドウマッピング用バインダー取得
    ShadowMapBinder *GetShadowMapBinder() const { return shadowMapBinder_; }
    /// @brief シャドウマッピング用カメラ同期コンポーネント取得
    ShadowMapCameraSync *GetShadowMapCameraSync() const { return shadowMapCameraSync_; }
    /// @brief シャドウマッピング用ライトカメラ取得
    Camera3D *GetLightCamera3D() const { return lightCamera3D_; }
    /// @brief ウィンドウ表示用2Dカメラ取得
    Camera2D *GetWindowCamera2D() const { return windowCamera2D_; }
    /// @brief メインウィンドウ取得
    Window *GetMainWindow() const { return mainWindow_; }

private:
    // ScreenBuffer
    ScreenBuffer *screenBuffer2D_ = nullptr;
    ScreenBuffer *screenBuffer3D_ = nullptr;
    Sprite *screenBuffer3DSprite_ = nullptr;
    Sprite *screenBuffer2DSprite_ = nullptr;

    // ScreenBuffer用メインカメラ
    Camera3D *mainCamera3D_ = nullptr;
    Camera2D *mainCamera2D_ = nullptr;

    // ScreenBufferアスペクト比維持コンポーネント
    ScreenBufferKeepRatio *keepRatioComp_ = nullptr;

    // コライダー
    ColliderComponent *colliderComp_ = nullptr;

    // 平行光源
    DirectionalLight *directionalLight_ = nullptr;

    // ライト管理用
    LightManager *lightManager_ = nullptr;

    // シャドウマッピング用
    ShadowMapBuffer *shadowMapBuffer_ = nullptr;
    ShadowMapBinder *shadowMapBinder_ = nullptr;
    ShadowMapCameraSync *shadowMapCameraSync_ = nullptr;
    Camera3D *lightCamera3D_ = nullptr;

    // Window表示用
    Camera2D *windowCamera2D_ = nullptr;
    Window *mainWindow_ = nullptr;
};

} // namespace KashipanEngine