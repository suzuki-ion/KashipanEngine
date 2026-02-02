#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Letterbox final : public ISceneComponent {
public:
    Letterbox()
        : ISceneComponent("Letterbox") {}

    ~Letterbox() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    void SetThickness(float thickness);
    void SetColor(const Vector4 &color);

private:
    void ApplyColor();
    void UpdateTransforms();

    ScreenBuffer *screenBuffer2D_ = nullptr;
    Rect *topRect_ = nullptr;
    Rect *bottomRect_ = nullptr;

    float thickness_ = 50.0f;
    Vector4 color_ = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
};

} // namespace KashipanEngine
