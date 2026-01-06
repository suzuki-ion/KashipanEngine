#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Assets/TextureManager.h"
#include "Graphics/ScreenBuffer.h"
#include "Objects/GameObjects/2D/Sprite.h"

namespace KashipanEngine {

class GameIntroLogoAnimation final : public ISceneComponent {
public:
    explicit GameIntroLogoAnimation(ScreenBuffer* screenBuffer)
        : ISceneComponent("GameIntroLogoAnimation", 1)
        , screenBuffer_(screenBuffer) {}

    ~GameIntroLogoAnimation() override = default;

    void Initialize() override;
    void Update() override;

    void Play();

private:
    void SetVisible(bool visible);

    ScreenBuffer* screenBuffer_ = nullptr;

    Sprite* logoSprite_ = nullptr;
    TextureManager::TextureHandle logoTexture_ = TextureManager::kInvalidHandle;

    bool playing_ = false;
    float elapsed_ = 0.0f;
};

} // namespace KashipanEngine
