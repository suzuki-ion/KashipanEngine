#pragma once
#if defined(RELEASE_BUILD)
#include <memory>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {
class GameEngine;
class DirectXCommon;
class Window;
/// Release-only, non-interactive text overlay. Owns a minimal ImGui renderer, without editor UI.
class RuntimeDebugOverlay final {
public:
    RuntimeDebugOverlay(Passkey<GameEngine>, DirectXCommon *directX);
    ~RuntimeDebugOverlay();
    void Draw(Window *window, bool showFps, bool showProfiling);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
#endif
