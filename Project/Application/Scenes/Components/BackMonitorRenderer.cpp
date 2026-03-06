#include "BackMonitorRenderer.h"
#include "BackMonitor.h"

namespace KashipanEngine {

BackMonitor* BackMonitorRenderer::GetBackMonitor() {
    if (!backMonitor_) {
        if (auto ctx = GetOwnerContext()) {
            backMonitor_ = ctx->GetComponent<BackMonitor>();
        }
    }
    return backMonitor_;
}

ScreenBuffer* BackMonitorRenderer::GetTargetScreenBuffer() {
    if (!target_) {
        if (auto *bm = GetBackMonitor()) {
            target_ = bm->GetScreenBuffer();
        }
    }
    return target_;
}

} // namespace KashipanEngine
