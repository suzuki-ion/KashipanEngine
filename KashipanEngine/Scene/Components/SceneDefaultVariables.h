#pragma once
#include <functional>

#include "Core/Window.h"
#include "Scene/Components/ISceneComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"

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
    /// @brief シャドウマッピング用バッファ取得
    ShadowMapBuffer *GetShadowMapBuffer() const { return shadowMapBuffer_; }
    /// @brief メインウィンドウ取得
    Window *GetMainWindow() const { return mainWindow_; }

private:
    // ScreenBuffer
    ScreenBuffer *screenBuffer2D_ = nullptr;
    ScreenBuffer *screenBuffer3D_ = nullptr;

    // シャドウマッピング用
    ShadowMapBuffer *shadowMapBuffer_ = nullptr;

    // Window表示用
    Window *mainWindow_ = nullptr;
};

} // namespace KashipanEngine