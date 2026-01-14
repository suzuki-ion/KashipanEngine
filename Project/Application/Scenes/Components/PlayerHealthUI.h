#pragma once

#include "Scene/Components/ISceneComponent.h"

#include <vector>

namespace KashipanEngine {

class Health;
class ScreenBuffer;
class Sprite;

class PlayerHealthUI final : public ISceneComponent {
public:
    PlayerHealthUI(ScreenBuffer *screenBuffer)
        : ISceneComponent("PlayerHealthUI", 1), screenBuffer_(screenBuffer) {}

    ~PlayerHealthUI() override = default;

    void SetHealth(Health *health);

    void Update() override;

private:
    void EnsureSprites();
    void UpdateSpriteColors();

    ScreenBuffer *screenBuffer_ = nullptr;
    Health *health_ = nullptr;

    int maxHpAtBind_ = 0;
    std::vector<Sprite *> sprites_;
};

} // namespace KashipanEngine
