#include "Scenes/Components/GameIntroLogoAnimation.h"
#include "Scene/SceneContext.h"

#include <algorithm>

namespace KashipanEngine {

namespace {
constexpr float kDuration = 2.0f;
constexpr float kLogoW = 1920.0f;
constexpr float kLogoH = 512.0f;

constexpr float kY = 540.0f;
constexpr float kXCenter = 960.0f;
constexpr float kXRightOutside = 1920.0f + kLogoW * 0.5f;
constexpr float kXLeftOutside = -kLogoW * 0.5f;
}

void GameIntroLogoAnimation::Initialize() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;
    sceneDefault_ = ctx->GetComponent<SceneDefaultVariables>("SceneDefaultVariables");

    ScreenBuffer* sb = sceneDefault_ ? sceneDefault_->GetScreenBuffer2D() : nullptr;

    auto attach = [&](Sprite* s) {
        if (!s || !sb) return;
        s->AttachToRenderer(sb, "Object2D.DoubleSidedCulling.BlendNormal");
    };

    logoTexture_ = TextureManager::GetTextureFromFileName("avoidAttacksText.png");

    {
        auto sp = std::make_unique<Sprite>();
        sp->SetUniqueBatchKey();
        sp->SetName("GameIntroLogo");
        sp->SetAnchorPoint(0.5f, 0.5f);

        if (auto* mat = sp->GetComponent2D<Material2D>()) {
            if (logoTexture_ != TextureManager::kInvalidHandle) {
                mat->SetTexture(logoTexture_);
            }
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 0.0f });
        }

        if (auto* tr = sp->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{ kXRightOutside, kY });
            tr->SetScale(Vector2{ kLogoW, -kLogoH });
        }

        if (sb) sp->AttachToRenderer(sb, "Object2D.DoubleSidedCulling.BlendNormal");
        logoSprite_ = sp.get();
        attach(logoSprite_);
        ctx->AddObject2D(std::move(sp));
    }

    playing_ = false;
    elapsed_ = 0.0f;
    SetVisible(false);
}

void GameIntroLogoAnimation::Play() {
    playing_ = true;
    elapsed_ = 0.0f;
    SetVisible(true);
}

void GameIntroLogoAnimation::SetVisible(bool visible) {
    if (!logoSprite_) return;
    if (auto* mat = logoSprite_->GetComponent2D<Material2D>()) {
        Vector4 c = mat->GetColor();
        c.w = visible ? 1.0f : 0.0f;
        mat->SetColor(c);
    }
}

void GameIntroLogoAnimation::Update() {
    if (!playing_) return;

    float dt = GetDeltaTime();
    if (dt >= 1.0f) dt = 0.0f;
    elapsed_ = std::min(elapsed_ + std::max(0.0f, dt), kDuration);

    const float t = std::clamp(elapsed_, 0.0f, kDuration);

    float x = kXCenter;
    if (t <= 1.0f) {
        const float u = std::clamp(t / 1.0f, 0.0f, 1.0f);
        x = EaseOutCubic(kXRightOutside, kXCenter, u);
    } else {
        const float u = std::clamp((t - 1.0f) / 1.0f, 0.0f, 1.0f);
        x = EaseInCubic(kXCenter, kXLeftOutside, u);
    }

    if (logoSprite_) {
        if (auto* tr = logoSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{ x, kY });
            tr->SetScale(Vector2{ kLogoW, kLogoH });
        }
    }

    if (elapsed_ >= kDuration) {
        playing_ = false;
        SetVisible(false);
    }
}

} // namespace KashipanEngine
