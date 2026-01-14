#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Assets/TextureManager.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Graphics/ScreenBuffer.h"

#include <array>

namespace KashipanEngine {

class Health;

class ResultUI final : public ISceneComponent {
public:
    ResultUI(
        ScreenBuffer *screenBuffer,
        Health *health)
        : ISceneComponent("ResultUI", 1)
        , screenBuffer_(screenBuffer)
        , health_(health) {}

    ~ResultUI() override = default;

    void Initialize() override;
    void Update() override;

    void ShowStart();

    void SetJustDodgeCount(int c) { justDodgeCount_ = (c < 0) ? 0 : c; }

    bool IsAnimating() const { return isAnimating_; }
    bool IsAnimationFinished() const { return isAnimationFinished_; }

private:
    void SetVisible(bool visible);

    ScreenBuffer *screenBuffer_ = nullptr;

    Health *health_ = nullptr;

    std::array<TextureManager::TextureHandle, 10> digitTextures_{};

    std::array<Sprite *, 5> scoreSprites_{};
    Sprite *scoreTextSprite_ = nullptr;
    Sprite *clearLogoSprite_ = nullptr;

    TextureManager::TextureHandle scoreTextTexture_ = TextureManager::kInvalidHandle;
    TextureManager::TextureHandle clearLogoTexture_ = TextureManager::kInvalidHandle;

    bool visible_ = false;

    float animTimeSec_ = 0.0f;
    bool isAnimating_ = false;
    bool isAnimationFinished_ = false;

    int justDodgeCount_ = 0;
};

} // namespace KashipanEngine
