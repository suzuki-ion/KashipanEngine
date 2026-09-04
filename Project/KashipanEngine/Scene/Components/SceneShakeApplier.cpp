#include "Scene/Components/SceneShakeApplier.h"

#include <algorithm>

#include "Debug/Logger.h"
#include "Objects/Components/Shake.h"

namespace KashipanEngine {

void SceneShakeApplier::RegisterShake(Shake *shake) {
    LogScope scope;
    if (!shake) return;
    if (std::find(shakes_.begin(), shakes_.end(), shake) != shakes_.end()) return;
    shakes_.push_back(shake);
}

void SceneShakeApplier::UnregisterShake(const Shake *shake) {
    LogScope scope;
    auto it = std::find(shakes_.begin(), shakes_.end(), shake);
    if (it != shakes_.end()) shakes_.erase(it);
}

void SceneShakeApplier::Update() {
    LogScope scope;
    for (Shake *shake : shakes_) {
        if (!shake) continue;
        // Immediateモードのシェイクは自身のUpdate内で既に処理済みのため、ここではスキップする
        if (shake->GetProcessTiming() != Shake::ProcessTiming::DeferredEnd) continue;
        shake->ApplyToTransformInterface(Passkey<SceneShakeApplier>());
    }
}

} // namespace KashipanEngine
