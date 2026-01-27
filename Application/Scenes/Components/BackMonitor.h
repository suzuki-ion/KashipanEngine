#pragma once
#include <KashipanEngine.h>
#include <memory>

namespace KashipanEngine {

class BackMonitor : public ISceneComponent {
public:
    BackMonitor();
    ~BackMonitor() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    ScreenBuffer *GetScreenBuffer() const { return screenBuffer_; }
    Camera2D *GetCamera2D() const { return camera2D_; }
    Camera3D *GetCamera3D() const { return camera3D_; }
    bool IsReady() const { return planeMoveCount_ >= 2; }

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Camera2D *camera2D_ = nullptr;
    Camera3D *camera3D_ = nullptr;
    Sprite *sprite_ = nullptr;
    Plane3D *plane3D_ = nullptr;
    Plane3D *planeBack_ = nullptr;

    /// @brief 板ポリの親オブジェクト
    Object3DBase *planeParent_ = nullptr;

    /// @brief プレーンを移動させるまでのカウント
    int planeMoveCount_ = 0;
};

} // namespace KashipanEngine
