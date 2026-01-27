#include "BackMonitorWithMenuScreen.h"
#include "BackMonitor.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Transform3D.h"

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

    // Create menu model objects (title, start, quit, credit)
    static Model* menuTitle = nullptr;
    static Model* menuStart = nullptr;
    static Model* menuQuit = nullptr;
    static Model* menuCredit = nullptr;

    float w = static_cast<float>(target->GetWidth());
    float h = static_cast<float>(target->GetHeight());
    // positions: place near center, stacked vertically
    const float centerX = w * 0.5f;
    const float baseY = h * 0.5f;
    const float gap = 2.0f; // vertical gap between menu entries
    const float depth = 4.0f;
    const Vector3 scaleVec{1.0f, 1.0f, 1.0f};

    if (!menuTitle) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuTitle.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuTitle");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{centerX, baseY - gap * 1.5f, depth});
            tr->SetScale(scaleVec);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        menuTitle = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    if (!menuStart) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuStart.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuStart");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{centerX, baseY - gap * 0.5f, depth});
            tr->SetScale(scaleVec);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        menuStart = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    if (!menuQuit) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuQuit.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuQuit");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{centerX, baseY + gap * 0.5f, depth});
            tr->SetScale(scaleVec);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        menuQuit = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    if (!menuCredit) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuCredit.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuCredit");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{centerX, baseY + gap * 1.5f, depth});
            tr->SetScale(scaleVec);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        menuCredit = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
}

} // namespace KashipanEngine
