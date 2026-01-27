#include "BackMonitorWithGameScreen.h"
#include "BackMonitor.h"

namespace KashipanEngine {

BackMonitorWithGameScreen::BackMonitorWithGameScreen(ScreenBuffer* target)
    : BackMonitorRenderer("BackMonitorWithGameScreen", target) {}

BackMonitorWithGameScreen::~BackMonitorWithGameScreen() {}

void BackMonitorWithGameScreen::Initialize() {}

void BackMonitorWithGameScreen::Update() {
    static Sprite *blitSprite = nullptr;
    if (!IsActive()) {
        if (blitSprite) {
            if (auto *mat = blitSprite->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
            }
        }
        return;
    }

    auto bm = GetBackMonitor();
    if (!bm) return;
    if (!bm->IsReady()) return;

    auto target = GetTargetScreenBuffer();
    if (!target) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

    // Example: render a copy of main 3D screen buffer if available
    auto sceneDefault = ctx->GetComponent<SceneDefaultVariables>();
    if (!sceneDefault) return;
    auto main3D = sceneDefault->GetScreenBuffer3D();
    if (!main3D) return;

    if (!blitSprite) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.GameScreenSprite");
        if (auto* tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(target->GetWidth());
            float h = static_cast<float>(target->GetHeight());
            tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
            tr->SetScale(Vector2{w, h});
        }
        if (auto* mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetTexture(main3D);
        }
        obj->AttachToRenderer(target, "Object2D.DoubleSidedCulling.BlendNormal");
        blitSprite = obj.get();
        ctx->AddObject2D(std::move(obj));
    } else {
        if (auto* mat = blitSprite->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetTexture(main3D);
        }
    }
}

} // namespace KashipanEngine
