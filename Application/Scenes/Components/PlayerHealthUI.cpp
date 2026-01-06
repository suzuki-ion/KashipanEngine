#include "Scenes/Components/PlayerHealthUI.h"

#include "Scene/SceneContext.h"

#include "Objects/Components/Health.h"

#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"

#include "Assets/TextureManager.h"

#include <algorithm>

namespace KashipanEngine {

void PlayerHealthUI::SetHealth(Health *health) {
    health_ = health;
    maxHpAtBind_ = health_ ? std::max(0, health_->GetHp()) : 0;
    EnsureSprites();
    UpdateSpriteColors();
}

void PlayerHealthUI::EnsureSprites() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    // Already created
    if (static_cast<int>(sprites_.size()) == maxHpAtBind_) return;

    // Clear old
    for (auto *s : sprites_) {
        if (s) ctx->RemoveObject2D(s);
    }
    sprites_.clear();

    const auto tex = TextureManager::GetTextureFromFileName("heart.png");

    for (int i = 0; i < maxHpAtBind_; ++i) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName(std::string("PlayerHealthUI_") + std::to_string(i));

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetTexture(tex);
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
        }

        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            const float size = 64.0f;
            const float margin = 16.0f;
            const float x = size * 0.5f + margin + static_cast<float>(i) * (size + margin);
            const float y = size * 0.5f + margin;
            tr->SetTranslate(Vector2{x, y});
            tr->SetScale(Vector2{size, -size});
        }

        auto *raw = obj.get();
        sprites_.push_back(raw);
        ctx->AddObject2D(std::move(obj));
    }
}

void PlayerHealthUI::UpdateSpriteColors() {
    const int hp = health_ ? std::max(0, health_->GetHp()) : 0;

    for (int i = 0; i < static_cast<int>(sprites_.size()); ++i) {
        auto *s = sprites_[i];
        if (!s) continue;
        auto *mat = s->GetComponent2D<Material2D>();
        if (!mat) continue;

        if (i < hp) {
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
        } else {
            mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 1.0f});
        }
    }
}

void PlayerHealthUI::Update() {
    EnsureSprites();
    UpdateSpriteColors();
}

} // namespace KashipanEngine
