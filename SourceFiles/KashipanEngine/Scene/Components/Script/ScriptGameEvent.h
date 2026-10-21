#pragma once
#include <atomic>
#include "Utilities/GameEvents.h"

namespace KashipanEngine {

/// @brief スクリプトの受信関数 `void 関数(GameEvent@ e)` へ渡すイベント（スクリプト側の GameEvent 型）
/// @details 配送中のメッセージのコピーを所有する参照カウント型。スクリプトがハンドルを保持し続けても安全。
///          値は型付き getter（GetInt / GetFloat / GetBool / GetString / GetVector2/3/4）で取り出す。
class ScriptGameEvent final {
public:
    explicit ScriptGameEvent(GameEvents::Message message) : message(std::move(message)) {}
    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() { if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this; }
    GameEvents::Message message;
private:
    ~ScriptGameEvent() = default;
    std::atomic<int> refCount_{1};
};

} // namespace KashipanEngine
