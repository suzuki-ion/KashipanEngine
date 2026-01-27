#include "BackMonitorWithMenuScreen.h"
#include "BackMonitor.h"

namespace KashipanEngine {

BackMonitorWithMenuScreen::BackMonitorWithMenuScreen(ScreenBuffer* target)
    : BackMonitorRenderer("BackMonitorWithMenuScreen", target) {}

BackMonitorWithMenuScreen::~BackMonitorWithMenuScreen() {}

void BackMonitorWithMenuScreen::Initialize() {}

void BackMonitorWithMenuScreen::Update() {
    if (!IsActive()) return;

    auto bm = GetBackMonitor();
    if (!bm) return;
    if (!bm->IsReady()) return;

    auto target = GetTargetScreenBuffer();
    if (!target) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

    static Sprite* panel = nullptr;
    if (!panel) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuPanel");
        if (auto* tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(target->GetWidth());
            float h = static_cast<float>(target->GetHeight());
            tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
            tr->SetScale(Vector2{w * 0.5f, h * 0.5f});
        }
        if (auto* mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.6f});
        }
        obj->AttachToRenderer(target, "Object2D.DoubleSidedCulling.BlendNormal");
        panel = obj.get();
        ctx->AddObject2D(std::move(obj));
    }
}

} // namespace KashipanEngine
