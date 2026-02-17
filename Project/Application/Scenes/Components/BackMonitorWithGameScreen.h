#pragma once
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class BackMonitorWithGameScreen : public BackMonitorRenderer {
public:
    BackMonitorWithGameScreen(ScreenBuffer* target);
    ~BackMonitorWithGameScreen() override;

    void Initialize() override;
    void Update() override;

private:
    Sprite *blitSprite_ = nullptr;
};

} // namespace KashipanEngine
