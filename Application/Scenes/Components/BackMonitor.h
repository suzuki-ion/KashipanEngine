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
    ScreenBuffer* screenBuffer_ = nullptr;
    Camera2D* camera2D_ = nullptr;
    Sprite* sprite_ = nullptr;
    Plane3D* plane3D_ = nullptr;

    /// @brief プレーンを移動させるまでのカウント
    int planeMoveCount_ = 0;
};

} // namespace KashipanEngine
