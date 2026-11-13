#pragma once
#if defined(RELEASE_BUILD)
#include <memory>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {
class GameEngine;
class DirectXCommon;
class Window;
/// Release版専用の操作不可なテキスト表示。エディターUIを含まない最小限のImGui描画機能を所有する。
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
