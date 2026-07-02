#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <cstdint>
#include "Utilities/FileIO.h"
#include "ComponentSerialize/ComponentRegistry.h"

#if defined(USE_IMGUI)
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

class Scene;
class SceneContext;

/// @brief シーンコンポーネントインターフェースクラス
class ISceneComponent {
    /// @brief コンポーネントの型ID設定用
    static inline size_t sComponentTypeID = 0;
public:
    /// @brief コンポーネントの型IDを取得
    /// @tparam T コンポーネントの型
    /// @return コンポーネントの型ID
    template<typename T>
    static size_t GetComponentTypeID() {
        static size_t typeID = sComponentTypeID++;
        return typeID;
    }

    virtual ~ISceneComponent() = default;
    ISceneComponent(const ISceneComponent &) = delete;
    ISceneComponent &operator=(const ISceneComponent &) = delete;
    ISceneComponent(ISceneComponent &&) = delete;
    ISceneComponent &operator=(ISceneComponent &&) = delete;

    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<ISceneComponent> Clone() const = 0;

    /// @brief コンポーネントの種類を取得
    const std::string &GetComponentType() const { return kComponentType_; }
    /// @brief 1つのシーンに登録可能な同じコンポーネントの最大数を取得
    size_t GetMaxComponentCountPerObject() const { return kMaxComponentCountPerObject_; }
    /// @brief コンポーネントの型IDを取得
    size_t GetComponentTypeID() const { return kComponentTypeID_; }
    /// @brief 更新優先度を取得
    int GetUpdatePriority() const { return updatePriority_; }
    /// @brief アクティブ状態を取得
    bool IsActive() const { return isActive_; }
    /// @brief アクティブ状態を設定
    void SetActive(bool active) { isActive_ = active; }

    /// @brief 初期化処理
    void InitializeInterface(Passkey<Scene>, SceneContext *sceneContext) {
        sceneContext_ = sceneContext;
        Initialize();
    }
    /// @brief 終了処理
    void FinalizeInterface(Passkey<Scene>) { Finalize(); }
    /// @brief 更新処理
    void UpdateInterface(Passkey<Scene>) { if (IsActive()) { Update(); } }

#ifdef USE_IMGUI
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowImGuiInterface(Passkey<Scene>) { ShowImGui(); }
#endif
    JSON SaveToJsonInterface(Passkey<Scene>) const { return SaveToJson(); }
    bool LoadFromJsonInterface(Passkey<Scene>, const JSON &json) { return LoadFromJson(json); }

protected:
    ISceneComponent(const std::string &typeName, size_t maxCount, size_t componentTypeID)
        : kComponentType_(typeName), kMaxComponentCountPerObject_(maxCount), kComponentTypeID_(componentTypeID), updatePriority_(1) {}
#define SCENE_COMPONENT_CONSTRUCTOR(typeName, maxCount, initializeCode) \
    typeName() : ISceneComponent(#typeName, maxCount, GetComponentTypeID<typeName>()) { REGISTER_COMPONENT_SCENE(typeName); initializeCode }

    /// @brief 初期化処理
    virtual void Initialize() {}
    /// @brief 終了処理
    virtual void Finalize() {}
    /// @brief 更新処理
    virtual void Update() {}

#if defined(USE_IMGUI)
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    virtual void ShowImGui() {
        ImGui::Text("None");
    }
#endif

    /// @brief コンポーネント情報をjsonへ保存
    /// @return コンポーネント情報を含むjsonオブジェクトを返す。保存する情報がない場合は空のjsonオブジェクトを返す
    virtual JSON SaveToJson() const { return JSON::object(); }
    /// @brief jsonからコンポーネント情報を読み込み
    /// @param json コンポーネント情報を含むjsonオブジェクト
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。読み込む情報がない場合は true を返す
    virtual bool LoadFromJson(const JSON &json) { (void)json; return true; }

    /// @brief 更新優先度を設定
    void SetUpdatePriority(int priority) { updatePriority_ = priority; }

    /// @brief 所属オブジェクトのシーンのコンテキストを取得
    SceneContext *GetOwnerSceneContext() const { return sceneContext_; }

private:
    /// @brief コンポーネントの種類名
    const std::string kComponentType_ = "IObjectComponent";
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数
    const size_t kMaxComponentCountPerObject_ = 0xFF;
    /// @brief コンポーネントの型ID
    const size_t kComponentTypeID_ = MAXSIZE_T;

    /// @brief 所属シーンのコンテキスト
    SceneContext *sceneContext_ = nullptr;

    /// @brief 更新優先度（小さいほど先に更新される）
    int updatePriority_ = 1;
    /// @brief アクティブ状態（falseの場合はUpdateが呼ばれない）
    bool isActive_ = true;
};

} // namespace KashipanEngine
