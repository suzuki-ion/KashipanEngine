#include "Scenes/Components/ResultUI.h"

#include "Scene/SceneContext.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/Health.h"

#include <algorithm>

namespace KashipanEngine {

void ResultUI::Initialize() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    auto attach = [&](Sprite *s) {
        if (!s || !screenBuffer_) return;
        s->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
    };

    // 数字テクスチャの取得
    for (size_t i = 0; i < digitTextures_.size(); ++i) {
        const std::string texName = "digit" + std::to_string(i) + ".png";
        digitTextures_[i] = TextureManager::GetTextureFromFileName(texName);
    }
    // score text / clear logo textures
    scoreTextTexture_ = TextureManager::GetTextureFromFileName("scoreText.png");
    clearLogoTexture_ = TextureManager::GetTextureFromFileName("clearLogo.png");
    
    // score sprites (5 digits)
    for (size_t i = 0; i < scoreSprites_.size(); ++i) {
        auto sp = std::make_unique<Sprite>();
        sp->SetUniqueBatchKey();
        sp->SetName(std::string("ResultScore_") + std::to_string(i));

        if (auto *mat = sp->GetComponent2D<Material2D>()) {
            mat->SetTexture(digitTextures_[0]);
        }

        scoreSprites_[i] = sp.get();
        attach(scoreSprites_[i]);
        ctx->AddObject2D(std::move(sp));
    }

    // score text sprite
    {
        auto sp = std::make_unique<Sprite>();
        sp->SetUniqueBatchKey();
        sp->SetName("ResultScoreText");

        if (auto *mat = sp->GetComponent2D<Material2D>()) {
            if (scoreTextTexture_ != TextureManager::kInvalidHandle) {
                mat->SetTexture(scoreTextTexture_);
            }
        }

        scoreTextSprite_ = sp.get();
        attach(scoreTextSprite_);
        ctx->AddObject2D(std::move(sp));
    }

    // clear logo sprite
    {
        auto sp = std::make_unique<Sprite>();
        sp->SetUniqueBatchKey();
        sp->SetName("ResultClearLogo");

        if (auto *mat = sp->GetComponent2D<Material2D>()) {
            if (clearLogoTexture_ != TextureManager::kInvalidHandle) {
                mat->SetTexture(clearLogoTexture_);
            }
        }

        clearLogoSprite_ = sp.get();
        attach(clearLogoSprite_);
        ctx->AddObject2D(std::move(sp));
    }

    SetVisible(false);
}

void ResultUI::ShowStart() {
    visible_ = true;
    animTimeSec_ = 0.0f;
    isAnimating_ = true;
    isAnimationFinished_ = false;
    SetVisible(true);
}

void ResultUI::SetVisible(bool visible) {
    const auto apply = [&](Sprite *s) {
        if (!s) return;
        if (auto *mat = s->GetComponent2D<Material2D>()) {
            Vector4 c = mat->GetColor();
            c.w = visible ? 0.0f : 0.0f;
            mat->SetColor(c);
        }
    };

    for (auto *s : scoreSprites_) apply(s);
    apply(scoreTextSprite_);
    apply(clearLogoSprite_);
}

void ResultUI::Update() {
    if (!visible_) return;

    const float dt = std::max(0.0f, GetDeltaTime());
    if (isAnimating_) {
        animTimeSec_ += dt;
        const float t = std::clamp(animTimeSec_ / 2.0f, 0.0f, 1.0f);

        const auto applyAlpha = [&](Sprite *s) {
            if (!s) return;
            if (auto *mat = s->GetComponent2D<Material2D>()) {
                Vector4 c = { 1.0f, 1.0f, 1.0f, t };
                mat->SetColor(c);
            }
        };

        for (auto *s : scoreSprites_) applyAlpha(s);
        applyAlpha(scoreTextSprite_);
        applyAlpha(clearLogoSprite_);

        if (t >= 1.0f) {
            isAnimating_ = false;
            isAnimationFinished_ = true;
        }
    }

    // Result score: justDodgeCount*100 + remainingHp*100
    int hp = 0;
    if (health_) {
        hp = std::max(0, health_->GetHp());
    }
    int v = (justDodgeCount_ * 100) + (hp * 100);
    v = std::clamp(v, 0, 99999);

    const float baseX = 960.0f;
    const float baseY = 540.0f;
    const float digitW = 128.0f;

    for (int i = static_cast<int>(scoreSprites_.size()) - 1; i >= 0; --i) {
        const int d = v % 10;
        v /= 10;

        Sprite *sp = scoreSprites_[static_cast<size_t>(i)];
        if (!sp) continue;

        if (auto *mat = sp->GetComponent2D<Material2D>()) {
            mat->SetTexture(digitTextures_[static_cast<size_t>(d)]);
        }
        if (auto *tr = sp->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{baseX + (static_cast<float>(i) - 2.0f) * digitW, baseY});
            tr->SetScale(Vector2{digitW, digitW});
        }
    }

    if (scoreTextSprite_) {
        if (auto *tr = scoreTextSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{ baseX, baseY + 100.0f });
            tr->SetScale(Vector2{300.0f, 128.0f});
        }
    }

    if (clearLogoSprite_) {
        if (auto *tr = clearLogoSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{ baseX, baseY + 340.0f });
            tr->SetScale(Vector2{1024.0f, 256.0f});
        }
    }
}

} // namespace KashipanEngine
