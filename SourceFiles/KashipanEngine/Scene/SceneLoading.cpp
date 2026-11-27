#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneFileIO.h"
#include "Scene/RenderTargetCarryOverRegistry.h"
#include "Core/ProjectPaths.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace KashipanEngine {

namespace {
constexpr float kMaxSceneLoadBudgetMilliseconds = 100.0f;
constexpr float kScenePreparationStartProgress = 0.25f;
constexpr float kScenePreparationProgressWeight = 0.7f;
} // namespace

SceneManager::~SceneManager() {
    shuttingDown_=true; CancelSceneLoad();
    currentScene_.reset();
    if(sActiveInstance_==this)sActiveInstance_=nullptr;
}
bool SceneManager::IsSceneLoading() const { return !queuedSceneLoadTarget_.empty() || sceneLoadState_==LoadState::Reading || sceneLoadState_==LoadState::Constructing; }
bool SceneManager::IsSceneLoadReady() const { return sceneLoadState_==LoadState::Ready && preparedScene_; }
float SceneManager::GetSceneLoadProgress() const {
    if(IsSceneLoadReady())return 1.f;
    if(sceneLoadState_==LoadState::Reading)return 0.f;
    if (sceneLoadState_ == LoadState::Constructing && preparedScene_) {
        return kScenePreparationStartProgress + kScenePreparationProgressWeight * preparedScene_->GetPreparationProgress();
    }
    return 0.f;
}
bool SceneManager::SetSceneLoadBudgetMilliseconds(float milliseconds) {
    if(!std::isfinite(milliseconds) || milliseconds<=0 || milliseconds > kMaxSceneLoadBudgetMilliseconds)return false;
    sceneLoadBudgetMilliseconds_=milliseconds; return true;
}
void SceneManager::RetireCompletedSceneReads() {
    for(auto it=retiredSceneReads_.begin();it!=retiredSceneReads_.end();) {
        if(it->wait_for(std::chrono::seconds(0))!=std::future_status::ready) { ++it; continue; }
        try { it->get(); }catch(...) { /* キャンセル済みの変更不可な I/O 結果を破棄する */ }
        it=retiredSceneReads_.erase(it);
    }
}
void SceneManager::CancelSceneLoad() {
    if(activatePreparedScene_ || !queuedSceneLoadTarget_.empty()) { hasPendingSceneChange_=false; pendingSceneName_.clear(); }
    activatePreparedScene_=false; queuedSceneLoadTarget_.clear();
    if(sceneRead_.valid())retiredSceneReads_.push_back(std::move(sceneRead_));
    preparedScene_.reset(); sceneLoadState_=LoadState::Idle;
    sceneLoadTarget_.clear(); sceneLoadError_.clear();
    RetireCompletedSceneReads();
}
bool SceneManager::BeginSceneLoad(const std::string &sceneName) {
    if(shuttingDown_)return false;
    const auto *entry=FindEntry(sceneName);
    if(!entry || hasPendingSceneChange_)return false;
    // メインスレッドでレジストリデータのスナップショットを取り、パスを解決する。シーンへのポインターはワーカーに渡さない。
    const auto path=entry->filePath.empty() ? std::string{} : ProjectPaths::ToPhysical(entry->filePath);
    JSON data=entry->factoryData;
    if(path.empty() && data.empty())data=JSON{{"sceneName",sceneName}};
    CancelSceneLoad(); sceneLoadTarget_=sceneName;
    try {
        if(!path.empty()) {
            sceneRead_=std::async(std::launch::async,[path] {
                auto result=LoadSceneFromPath(path);
                if(result.is_discarded() || !result.is_object() || result.empty())throw std::runtime_error("Could not read scene: "+path);
                return result;
            });
            sceneLoadState_=LoadState::Reading;
        } else {
            preparedScene_=std::make_unique<Scene>(sceneName,true);
            preparedScene_->SetSceneManager(Passkey<SceneManager>{},this);
            preparedScene_->BeginPreparation(Passkey<SceneManager>{},std::move(data));
            sceneLoadState_=LoadState::Constructing;
        }
        return true;
    }catch(const std::exception &e) { sceneLoadError_=e.what(); sceneLoadState_=LoadState::Failed; preparedScene_.reset(); return false; }
}
bool SceneManager::ChangeSceneWithLoading(const std::string &target,const std::string &loadingScene) {
    if(target==loadingScene || !FindEntry(target) || !FindEntry(loadingScene))return false;
    if(!ChangeScene(loadingScene))return false;
    queuedSceneLoadTarget_=target; sceneLoadTarget_=target;
    return true;
}
void SceneManager::AdvanceSceneLoad() {
    RetireCompletedSceneReads();
    try {
        if(sceneLoadState_==LoadState::Reading && sceneRead_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            auto data=sceneRead_.get();
            preparedScene_=std::make_unique<Scene>(sceneLoadTarget_,true);
            preparedScene_->SetSceneManager(Passkey<SceneManager>{},this);
            preparedScene_->BeginPreparation(Passkey<SceneManager>{},std::move(data));
            sceneLoadState_=LoadState::Constructing;
            return; // 構築を始める前に、ロード画面を少なくとも 1 フレーム表示する。
        }
        if(sceneLoadState_!=LoadState::Constructing)return;
        if(preparedScene_->AdvancePreparation(Passkey<SceneManager>{},sceneLoadBudgetMilliseconds_))sceneLoadState_=LoadState::Ready;
        else if(!preparedScene_->GetPreparationError().empty())throw std::runtime_error(preparedScene_->GetPreparationError());
    }catch(const std::exception &e) {
        sceneLoadError_=e.what(); sceneLoadState_=LoadState::Failed;
        preparedScene_.reset(); // 失敗した場合は、現在のロードシーンを維持する。
    }
}
bool SceneManager::ActivateLoadedScene() {
    if(shuttingDown_ || !IsSceneLoadReady() || hasPendingSceneChange_)return false;
    activatePreparedScene_=true; hasPendingSceneChange_=true; pendingSceneName_=sceneLoadTarget_; return true;
}
bool SceneManager::CommitPreparedScene() {
    if(!activatePreparedScene_ || !IsSceneLoadReady())return false;
    hasPendingSceneChange_=false; activatePreparedScene_=false; pendingSceneName_.clear();
    const bool wasPlaying=currentScene_ && currentScene_->IsPlaying();
    auto incoming=std::move(preparedScene_);
    // ロードシーンの Finalize や End から、新たなシーン遷移が要求される場合がある。
    // 切り替え処理中のシーンは、書き換え可能なロードジョブのスロットの外で保持する。
    sceneLoadState_=LoadState::Idle; sceneLoadTarget_.clear(); sceneLoadError_.clear();
    RenderTargetCarryOverRegistry::BeginSceneSwitch(Passkey<SceneManager>{});
    struct CarryGuard { ~CarryGuard() { RenderTargetCarryOverRegistry::EndSceneSwitch(Passkey<SceneManager>{}); } } guard;
    if(currentScene_)currentScene_->FinalizeInterface(Passkey<SceneManager>{});
    currentScene_.reset();
    currentScene_=std::move(incoming);
#if defined(USE_IMGUI)
    if(wasPlaying)currentScene_->ContinuePlayAfterSceneChange(Passkey<SceneManager>{});
#else
    (void)wasPlaying;
#endif
    auto snapshot=currentScene_->ActivatePreparation(Passkey<SceneManager>{});
    currentScene_->InitializeInterface(Passkey<SceneManager>{});
    preCrashSnapshot_=std::move(snapshot); preCrashSnapshotElapsedTime_=0;
    recoverySnapshot_={}; retiredRecoverySnapshot_=JSON();
    return true;
}

bool SceneContext::BeginSceneLoad(const std::string &name) { auto *m=owner_->GetSceneManager(); return !IsPreparing() && m && m->BeginSceneLoad(name); }
bool SceneContext::ChangeSceneWithLoading(const std::string &target,const std::string &loadingScene) { auto *m=owner_->GetSceneManager(); return !IsPreparing() && m && m->ChangeSceneWithLoading(target,loadingScene); }
bool SceneContext::IsSceneLoading() const { auto *m=owner_->GetSceneManager(); return m && m->IsSceneLoading(); }
bool SceneContext::IsSceneLoadReady() const { auto *m=owner_->GetSceneManager(); return m && m->IsSceneLoadReady(); }
float SceneContext::GetSceneLoadProgress() const { auto *m=owner_->GetSceneManager(); return m ? m->GetSceneLoadProgress() : 0; }
std::string SceneContext::GetSceneLoadError() const { auto *m=owner_->GetSceneManager(); return m ? m->GetSceneLoadError() : std::string{}; }
std::string SceneContext::GetSceneLoadTarget() const { auto *m=owner_->GetSceneManager(); return m ? m->GetSceneLoadTarget() : std::string{}; }
bool SceneContext::ActivateLoadedScene() { auto *m=owner_->GetSceneManager(); return !IsPreparing() && m && m->ActivateLoadedScene(); }
void SceneContext::CancelSceneLoad() { if(auto *m=owner_->GetSceneManager(); m && !IsPreparing())m->CancelSceneLoad(); }
bool SceneContext::SetSceneLoadBudgetMilliseconds(float milliseconds) { auto *m=owner_->GetSceneManager(); return !IsPreparing() && m && m->SetSceneLoadBudgetMilliseconds(milliseconds); }
}
