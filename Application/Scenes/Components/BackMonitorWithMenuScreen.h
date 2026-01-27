#pragma once
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class BackMonitorWithMenuScreen : public BackMonitorRenderer {
public:
    BackMonitorWithMenuScreen(ScreenBuffer* target);
    ~BackMonitorWithMenuScreen() override;

    void Initialize() override;
    void Update() override;
};

} // namespace KashipanEngine
