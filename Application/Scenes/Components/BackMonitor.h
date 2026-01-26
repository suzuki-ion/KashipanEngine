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
