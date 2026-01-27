#include "BackMonitorWithMenuScreen.h"

namespace KashipanEngine {

BackMonitorWithMenuScreen::BackMonitorWithMenuScreen(ScreenBuffer* target)
    : BackMonitorRenderer("BackMonitorWithMenuScreen", target) {}

BackMonitorWithMenuScreen::~BackMonitorWithMenuScreen() {}

void BackMonitorWithMenuScreen::Initialize() {
    // Initialize menu elements if needed
}

void BackMonitorWithMenuScreen::Update() {
    if (!target_) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

    // Simple menu rendering: draw a translucent rectangle and some text
    static Sprite* panel = nullptr;
    if (!panel) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuPanel");
        if (auto* tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(target_->GetWidth());
            float h = static_cast<float>(target_->GetHeight());
            tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
            tr->SetScale(Vector2{w * 0.5f, h * 0.5f});
        }
        if (auto* mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.6f});
        }
        obj->AttachToRenderer(target_, "Object2D.DoubleSidedCulling.BlendNormal");
        panel = obj.get();
        ctx->AddObject2D(std::move(obj));
    }
}

} // namespace KashipanEngine
