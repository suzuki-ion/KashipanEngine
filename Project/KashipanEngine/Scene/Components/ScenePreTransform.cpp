#include "ScenePreTransform.h"

#include <algorithm>

#include "Debug/Logger.h"
#include "Objects/Components/PreTransform.h"

namespace KashipanEngine {

void ScenePreTransform::RegisterPreTransform(PreTransform *preTransform) {
    LogScope scope;
    if (!preTransform) return;
    if (std::find(registeredPreTransforms_.begin(), registeredPreTransforms_.end(), preTransform) != registeredPreTransforms_.end()) return;
    registeredPreTransforms_.push_back(preTransform);
}

void ScenePreTransform::UnregisterPreTransform(const PreTransform *preTransform) {
    LogScope scope;
    auto it = std::find(registeredPreTransforms_.begin(), registeredPreTransforms_.end(), preTransform);
    if (it != registeredPreTransforms_.end()) registeredPreTransforms_.erase(it);
}

void ScenePreTransform::Update() {
    LogScope scope;
    // 更新優先度をSceneRendererより大きくしているため、このフレームの描画が終わった後に
    // 現在値を「前フレームの値」として書き込む（次フレームに向けた更新）
    for (auto *preTransform : registeredPreTransforms_) {
        if (!preTransform || !preTransform->IsActive()) continue;
        preTransform->CaptureCurrentAsPrevious();
    }
}

void ScenePreTransform::Finalize() {
    LogScope scope;
    registeredPreTransforms_.clear();
}

} // namespace KashipanEngine
