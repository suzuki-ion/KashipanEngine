#include "Scene/RenderTargetCarryOverRegistry.h"
#include "Debug/Logger.h"
#include <cstdio>

namespace KashipanEngine {

namespace {
// ログ用に種別名を文字列化する（デバッグ調査用。ScreenBuffer/ShadowMapBuffer等の
// 描画先が「シーン切り替えを跨いで正しく引き継がれているか」を追跡するために使う）
const char *KindToString(RenderTargetCarryOverRegistry::Kind kind) {
    switch (kind) {
        case RenderTargetCarryOverRegistry::Kind::ScreenBuffer: return "ScreenBuffer";
        case RenderTargetCarryOverRegistry::Kind::NormalWindow: return "NormalWindow";
        case RenderTargetCarryOverRegistry::Kind::OverlayWindow: return "OverlayWindow";
    }
    return "Unknown";
}

// 「種別 / キー (ポインタ)」形式のログ末尾文字列を組み立てる。ポインタを含めることで、
// シーン切り替え・Play/Stopを跨いで同一インスタンスが使い回されているか、
// それとも別インスタンスに置き換わっているかをログだけから追跡できるようにする
std::string DescribeEntry(RenderTargetCarryOverRegistry::Kind kind, const std::string &key, const void *resource) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%p", resource);
    return std::string(KindToString(kind)) + " / " + key + " (" + buf + ")";
}
} // namespace

void RenderTargetCarryOverRegistry::BeginSceneSwitch(Passkey<SceneManager>) {
    LogScope scope;
    sSceneSwitchInProgress_ = true;
    Log(Translation("engine.rendertargetcarryover.beginsceneswitch"), LogSeverity::Debug);
}

void RenderTargetCarryOverRegistry::EndSceneSwitch(Passkey<SceneManager>) {
    LogScope scope;
    sSceneSwitchInProgress_ = false;
    // 引き取られなかったリソースはここで実際に破棄する。ここに残っているということは
    // 「同名のコンポーネントが新シーン側に存在しない（名前が食い違っている等）」ことを
    // 意味するため、意図しない名前の不一致を洗い出せるようログに残す
    for (size_t poolIndex = 0; poolIndex < 3; ++poolIndex) {
        auto &pool = sPools_[poolIndex];
        for (auto &[key, entry] : pool) {
            Log(Translation("engine.rendertargetcarryover.endsceneswitch.unclaimed") +
                DescribeEntry(static_cast<Kind>(poolIndex), key, entry.resource), LogSeverity::Warning);
            if (entry.destroyFn) entry.destroyFn();
        }
        pool.clear();
    }
}

bool RenderTargetCarryOverRegistry::IsSceneSwitchInProgress() {
    return sSceneSwitchInProgress_;
}

void RenderTargetCarryOverRegistry::Deposit(Kind kind, const std::string &key, void *resource, std::function<void()> destroyFn) {
    LogScope scope;
    if (!resource || !destroyFn) return;
    if (key.empty() || !sSceneSwitchInProgress_) {
        // 切り替え中でない場合は引き継ぎを行わず即座に破棄する（リーク防止）
        Log(Translation("engine.rendertargetcarryover.deposit.destroyed") +
            DescribeEntry(kind, key, resource), LogSeverity::Debug);
        destroyFn();
        return;
    }
    auto &pool = sPools_[PoolIndex(kind)];
    // 同名の未引き継ぎエントリが既にある場合は上書き前に実際に破棄しておく
    auto it = pool.find(key);
    if (it != pool.end() && it->second.destroyFn) {
        it->second.destroyFn();
    }
    Log(Translation("engine.rendertargetcarryover.deposit.carried") +
        DescribeEntry(kind, key, resource), LogSeverity::Debug);
    pool[key] = Entry{ resource, std::move(destroyFn) };
}

void *RenderTargetCarryOverRegistry::Claim(Kind kind, const std::string &key) {
    LogScope scope;
    if (key.empty()) return nullptr;
    auto &pool = sPools_[PoolIndex(kind)];
    auto it = pool.find(key);
    if (it == pool.end()) return nullptr;
    void *resource = it->second.resource;
    pool.erase(it);
    Log(Translation("engine.rendertargetcarryover.claim.success") +
        DescribeEntry(kind, key, resource), LogSeverity::Debug);
    return resource;
}

} // namespace KashipanEngine
