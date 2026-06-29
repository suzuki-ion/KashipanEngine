#pragma once
#pragma once

#include <array>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#ifdef USE_IMGUI
#include "Scene/SceneEditor.h"
#endif
#include "Objects/EmptyObject.h"
#include "Objects/Collision/Collider.h"
#include "Scene/Components/ISceneComponent.h"
#include "Utilities/Passkeys.h"

#include "Assets/ModelManager.h"

namespace KashipanEngine {

class SceneContext;
#ifdef USE_IMGUI
class SceneEditorContext;
#endif

class SceneManager;
class GameEngine;

class AudioManager;
class SkeletonManager;
class SamplerManager;
class TextureManager;
class AnimationManager;
class Input;
class InputCommand;

class SceneBase {
public:
    SceneBase() = delete;
    virtual ~SceneBase();

    SceneBase(const SceneBase &) = delete;
    SceneBase &operator=(const SceneBase &) = delete;
    SceneBase(SceneBase &&) = delete;
    SceneBase &operator=(SceneBase &&) = delete;

    const std::string &GetName() const { return name_; }
    const std::string &GetNextSceneName() const { return nextSceneName_; }

    void Update();

#if defined(USE_IMGUI)
    void ShowImGui();
#endif

    void SetSceneManager(Passkey<SceneManager>, SceneManager *sceneManager) { sceneManager_ = sceneManager; }

    static void SetEnginePointers(
        Passkey<GameEngine>,
        AudioManager *audioManager,
        ModelManager *modelManager,
        SkeletonManager *skeletonManager,
        SamplerManager *samplerManager,
        TextureManager *textureManager,
        AnimationManager *animationManager,
        Input *input,
        InputCommand *inputCommand);

    /// @brief シーン初期化（SceneManager から呼ばれる）
    virtual void Initialize() {}

    /// @brief シーン終了処理（SceneManager から呼ばれる）
    virtual void Finalize() {}

    friend class SceneContext;
#ifdef USE_IMGUI
    friend class SceneEditorContext;
#endif

protected:
    SceneBase(const std::string &sceneName);

    virtual void OnUpdate() {}

    /// @brief シーン変数を取得
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param out 変数の値を格納する参照
    /// @return 変数が存在する場合は true、存在しない場合は false を返す
    template<typename T>
    bool TryGetSceneVariable(const std::string &key, T &out) const {
        const auto &vars = GetSceneVariables();
        if (!vars.contains(key)) return false;
        return vars.at(key).TryGetValue(out);
    }
    /// @brief シーン変数を取得（存在しない場合はデフォルト値を返す）
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param defaultValue デフォルト値
    /// @return 変数の値
    template<typename T>
    T GetSceneVariableOr(const std::string &key, const T &defaultValue) const {
        T v = defaultValue;
        (void)TryGetSceneVariable<T>(key, v);
        return v;
    }

    /// @brief 空のオブジェクトを生成
    /// @param name 空のオブジェクト名
    /// @param index 生成位置のインデックス（省略時は末尾に追加）
    /// @return 生成された空のオブジェクトのポインタ
    EmptyObject *CreateEmptyObject(const std::string &name = "", size_t index = MAXSIZE_T);
    /// @brief オブジェクトを削除
    /// @param obj 削除するオブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool DeleteObject(EmptyObject *obj);
    /// @brief 3D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 3D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject(EmptyObject *obj);
    /// @brief オブジェクトを移動
    /// @param 移動するオブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject(EmptyObject *obj, size_t newIndex);

    /// @brief シーン内のオブジェクト一覧を取得
    /// @return オブジェクトのリスト
    const std::vector<std::unique_ptr<EmptyObject>> &GetSceneObjects() const { return objects_; }
    /// @brief 名前から一致するオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<EmptyObject *> GetSceneObjects(const std::string &objectName) const;
    /// @brief 名前から一致する最初のオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const std::string &objectName) const;
    /// @brief ポインタから一致するオブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(EmptyObject *obj) const;
    /// @brief UUIDから一致するオブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const UUID128 &uuid) const;

    /// @brief シーン内のオブジェクトをすべて削除
    void ClearSceneObjects();

    /// @brief 次のシーン名を設定
    /// @param nextSceneName 次のシーン名
    void SetNextSceneName(const std::string &nextSceneName) { nextSceneName_ = nextSceneName; }
    /// @brief 次のシーンに切り替え
    void ChangeToNextScene();
    /// @brief 次のシーン名をクリア
    void ClearNextSceneName() { nextSceneName_.clear(); }

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
    std::vector<T *> GetSceneComponents() const {
        std::vector<T *> components;
        auto range = sceneComponentsIndexByType_.equal_range(std::type_index(typeid(T)));
        for (auto it = range.first; it != range.second; ++it) {
            components.push_back(static_cast<T *>(sceneComponents_[it->second].get()));
        }
        return components;
    }

    template<typename T>
    T *GetSceneComponent() const {
        auto range = sceneComponentsIndexByType_.equal_range(std::type_index(typeid(T)));
        if (range.first != range.second) {
            return static_cast<T *>(sceneComponents_[range.first->second].get());
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<ISceneComponent>> &GetSceneComponents() const { return sceneComponents_; }

    size_t HasSceneComponents(const std::string &componentName) const {
        return sceneComponentsIndexByName_.count(componentName);
    }

    void AddSceneVariable(const std::string &key, const std::any &value);
    const MyStd::AnyUnorderedMap &GetSceneVariables() const;

    SceneContext *GetSceneContext() const { return sceneContext_.get(); }
#ifdef USE_IMGUI
    SceneEditorContext *GetSceneEditorContext() const { return sceneEditorContext_.get(); }
#endif

    static AudioManager *GetAudioManager() { return sAudioManager; }
    static ModelManager *GetModelManager() { return sModelManager; }
    static SkeletonManager *GetSkeletonManager() { return sSkeletonManager; }
    static SamplerManager *GetSamplerManager() { return sSamplerManager; }
    static TextureManager *GetTextureManager() { return sTextureManager; }
    static AnimationManager *GetAnimationManager() { return sAnimationManager; }
    static Input *GetInput() { return sInput; }
    static InputCommand *GetInputCommand() { return sInputCommand; }

private:
    static inline AudioManager *sAudioManager = nullptr;
    static inline ModelManager *sModelManager = nullptr;
    static inline SkeletonManager *sSkeletonManager = nullptr;
    static inline SamplerManager *sSamplerManager = nullptr;
    static inline TextureManager *sTextureManager = nullptr;
    static inline AnimationManager *sAnimationManager = nullptr;
    static inline Input *sInput = nullptr;
    static inline InputCommand *sInputCommand = nullptr;

    void RebuildObjectIndexTables();

    std::string name_;

    std::vector<std::unique_ptr<EmptyObject>> objects_;
    std::unordered_map<UUID128, size_t> objectsIndexByUUID_;
    std::unordered_map<EmptyObject *, size_t> objectsIndexByPointer_;
    std::unordered_map<std::string, std::vector<size_t>> objectsIndexByName_;

    std::vector<std::unique_ptr<ISceneComponent>> sceneComponents_;
    std::unordered_map<size_t, std::vector<size_t>> sceneComponentsIndexByType_;

    std::unique_ptr<SceneContext> sceneContext_;
#ifdef USE_IMGUI
    std::unique_ptr<SceneEditorContext> sceneEditorContext_;
    std::unique_ptr<SceneEditor> sceneEditor_;
#endif

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
