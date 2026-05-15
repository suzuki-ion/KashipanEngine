#pragma once

#include <any>
#include <string>
#include <vector>

#include "Scene/SceneBase.h"

namespace KashipanEngine {

/// @brief Scene コンテキスト（SceneComponent 用）
class SceneContext final {
public:
    SceneContext(Passkey<SceneBase>, SceneBase *owner) : owner_(owner) {}
    ~SceneContext() = default;

    SceneContext(const SceneContext &) = delete;
    SceneContext &operator=(const SceneContext &) = delete;
    SceneContext(SceneContext &&) = delete;
    SceneContext &operator=(SceneContext &&) = delete;

    SceneBase *GetOwner() const { return owner_; }

    const std::string &GetName() const;

    /// @brief シーン変数を追加または上書き
    void AddSceneVariable(const std::string &key, const std::any &value);

    /// @brief シーン変数一覧を取得
    const MyStd::AnyUnorderedMap &GetSceneVariables() const;

    template<typename T>
    bool TryGetSceneVariable(const std::string &key, T &out) const {
        const auto &vars = GetSceneVariables();
        if (!vars.contains(key)) return false;
        return vars.at(key).TryGetValue(out);
    }

    template<typename T>
    T GetSceneVariableOr(const std::string &key, const T &defaultValue) const {
        T v = defaultValue;
        (void)TryGetSceneVariable<T>(key, v);
        return v;
    }

    AudioManager *GetAudioManager() { return SceneBase::GetAudioManager(); }
    ModelManager *GetModelManager() { return SceneBase::GetModelManager(); }
    SamplerManager *GetSamplerManager() { return SceneBase::GetSamplerManager(); }
    TextureManager *GetTextureManager() { return SceneBase::GetTextureManager(); }
    Input *GetInput() { return SceneBase::GetInput(); }
    InputCommand *GetInputCommand() { return SceneBase::GetInputCommand(); }

private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine
