#include "Scene/RenderTargetCarryOverRegistry.h"

namespace KashipanEngine {

void RenderTargetCarryOverRegistry::BeginSceneSwitch(Passkey<SceneManager>) {
    sSceneSwitchInProgress_ = true;
}

void RenderTargetCarryOverRegistry::EndSceneSwitch(Passkey<SceneManager>) {
    sSceneSwitchInProgress_ = false;
    // 引き取られなかったリソースはここで実際に破棄する
    for (auto &pool : sPools_) {
        for (auto &[key, entry] : pool) {
            if (entry.destroyFn) entry.destroyFn();
        }
        pool.clear();
    }
}

bool RenderTargetCarryOverRegistry::IsSceneSwitchInProgress() {
    return sSceneSwitchInProgress_;
}

void RenderTargetCarryOverRegistry::Deposit(Kind kind, const std::string &key, void *resource, std::function<void()> destroyFn) {
    if (!resource || !destroyFn) return;
    if (key.empty() || !sSceneSwitchInProgress_) {
        // 切り替え中でない場合は引き継ぎを行わず即座に破棄する（リーク防止）
        destroyFn();
        return;
    }
    auto &pool = sPools_[PoolIndex(kind)];
    // 同名の未引き継ぎエントリが既にある場合は上書き前に実際に破棄しておく
    auto it = pool.find(key);
    if (it != pool.end() && it->second.destroyFn) {
        it->second.destroyFn();
    }
    pool[key] = Entry{ resource, std::move(destroyFn) };
}

void *RenderTargetCarryOverRegistry::Claim(Kind kind, const std::string &key) {
    if (key.empty()) return nullptr;
    auto &pool = sPools_[PoolIndex(kind)];
    auto it = pool.find(key);
    if (it == pool.end()) return nullptr;
    void *resource = it->second.resource;
    pool.erase(it);
    return resource;
}

} // namespace KashipanEngine
