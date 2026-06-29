#pragma once

#include <any>
#include <string>
#include <type_traits>
#include <vector>

#include "Scene/SceneBase.h"

namespace KashipanEngine {

/// @brief Scene コンテキスト
class SceneContext final {
public:
    SceneContext(Passkey<SceneBase>, SceneBase *owner) : owner_(owner) {}
    ~SceneContext() = default;

    SceneContext(const SceneContext &) = delete;
    SceneContext &operator=(const SceneContext &) = delete;
    SceneContext(SceneContext &&) = delete;
    SceneContext &operator=(SceneContext &&) = delete;

    SceneBase *GetOwner() const { return owner_; }

    /// @brief シーン変数を取得
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param out 変数の値を格納する参照
    /// @return 変数が存在する場合は true、存在しない場合は false を返す
    template<typename T>
    bool TryGetSceneVariable(const std::string &key, T &out) const { return owner_->TryGetSceneVariable<T>(key, out); }
    /// @brief シーン変数を取得（存在しない場合はデフォルト値を返す）
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param defaultValue デフォルト値
    /// @return 変数の値
    template<typename T>
    T GetSceneVariableOr(const std::string &key, const T &defaultValue) const { return owner_->GetSceneVariableOr<T>(key, defaultValue); }

    /// @brief 空のオブジェクトを生成
    /// @param name 空のオブジェクト名
    /// @param index 生成位置のインデックス（省略時は末尾に追加）
    /// @return 生成された空のオブジェクトのポインタ
    EmptyObject *CreateEmptyObject(const std::string &name = "", size_t index = MAXSIZE_T) { return owner_->CreateEmptyObject(name, index); }
    /// @brief オブジェクトを削除
    /// @param obj 削除するオブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool DeleteObject(EmptyObject *obj) { return owner_->DeleteObject(obj); }
    /// @brief 3D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 3D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject(EmptyObject *obj) { return owner_->ReleaseObject(obj); }
    /// @brief オブジェクトを移動
    /// @param 移動するオブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject(EmptyObject *obj, size_t newIndex) { return owner_->MoveObject(obj, newIndex); }

    /// @brief シーン内のオブジェクト一覧を取得
    /// @return オブジェクトのリスト
    const std::vector<std::unique_ptr<EmptyObject>> &GetSceneObjects() const { return owner_->GetSceneObjects(); }
    /// @brief 名前から一致するオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<EmptyObject *> GetSceneObjects(const std::string &objectName) const { return owner_->GetSceneObjects(objectName); }
    /// @brief 名前から一致する最初のオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const std::string &objectName) const { return owner_->GetSceneObject(objectName); }
    /// @brief ポインタから一致するオブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(EmptyObject *obj) const { return owner_->GetSceneObject(obj); }
    /// @brief UUIDから一致するオブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const UUID128 &uuid) const { return owner_->GetSceneObject(uuid); }

    /// @brief 次のシーン名を設定
    /// @param nextSceneName 次のシーン名
    void SetNextSceneName(const std::string &nextSceneName) { return owner_->SetNextSceneName(nextSceneName); }
    /// @brief 次のシーンに切り替え
    void ChangeToNextScene() { return owner_->ChangeToNextScene(); }
    /// @brief 次のシーン名をクリア
    void ClearNextSceneName() { return owner_->ClearNextSceneName(); }

    bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp);
    bool RemoveSceneComponent(ISceneComponent *comp);
    void ClearSceneComponents();

    std::vector<ISceneComponent *> GetSceneComponents(const std::string &componentName) const {
        std::vector<ISceneComponent *> components;
        auto range = sceneComponentsIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            components.push_back(sceneComponents_[it->second].get());
        }
        return components;
    }

    ISceneComponent *GetSceneComponent(const std::string &componentName) const {
        auto range = sceneComponentsIndexByName_.equal_range(componentName);
        if (range.first != range.second) {
            return sceneComponents_[range.first->second].get();
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T *> GetSceneComponents() const { return owner_->GetSceneComponents<T>(); }
    template<typename T>
    T *GetSceneComponent() const { return owner_->GetSceneComponent<T>(); }
    const std::vector<std::unique_ptr<ISceneComponent>> &GetSceneComponents() const { return owner_->GetSceneComponents(); }
    size_t HasSceneComponents(const std::string &componentName) const { return owner_->HasSceneComponents(componentName); }
    void AddSceneVariable(const std::string &key, const std::any &value) { owner_->AddSceneVariable(key, value); }
    const MyStd::AnyUnorderedMap &GetSceneVariables() const { return owner_->GetSceneVariables(); }

    AudioManager *GetAudioManager() { return SceneBase::GetAudioManager(); }
    ModelManager *GetModelManager() { return SceneBase::GetModelManager(); }
    SkeletonManager *GetSkeletonManager() { return SceneBase::GetSkeletonManager(); }
    SamplerManager *GetSamplerManager() { return SceneBase::GetSamplerManager(); }
    TextureManager *GetTextureManager() { return SceneBase::GetTextureManager(); }
    AnimationManager *GetAnimationManager() { return SceneBase::GetAnimationManager(); }
    Input *GetInput() { return SceneBase::GetInput(); }
    InputCommand *GetInputCommand() { return SceneBase::GetInputCommand(); }

private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine
