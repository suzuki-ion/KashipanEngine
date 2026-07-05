#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <cstdint>
#include "Utilities/FileIO.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Utilities/MyAny.h"
#if defined(USE_IMGUI)
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

class EmptyObject;
class ObjectContext;
class SceneContext;

/// @brief オブジェクトコンポーネントインターフェースクラス
class IObjectComponent {
    /// @brief コンポーネントの型ID設定用
    static inline size_t sComponentTypeID = 0;
public:
    /// @brief メンバー変数の情報（ImGuiなどからのアクセス用）
    struct MemberVariable {
        void *ptr = nullptr;
        TypeInfo typeInfo;
    };
    /// @brief コンポーネントの型IDを取得
    /// @tparam T コンポーネントの型
    /// @return コンポーネントの型ID
    template<typename T>
    static size_t GetComponentTypeID() {
        static size_t typeID = sComponentTypeID++;
        return typeID;
    }

    virtual ~IObjectComponent() = default;
    IObjectComponent(const IObjectComponent &) = delete;
    IObjectComponent &operator=(const IObjectComponent &) = delete;
    IObjectComponent(IObjectComponent &&) = delete;
    IObjectComponent &operator=(IObjectComponent &&) = delete;

    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IObjectComponent> Clone() const = 0;

    /// @brief コンポーネントの種類を取得
    const std::string &GetComponentType() const { return kComponentType_; }
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数を取得
    size_t GetMaxComponentCountPerObject() const { return kMaxComponentCountPerObject_; }
    /// @brief コンポーネントの型IDを取得
    size_t GetComponentTypeID() const { return kComponentTypeID_; }
    /// @brief 更新優先度を取得
    int GetUpdatePriority() const { return updatePriority_; }
    /// @brief 更新優先度を設定
    void SetUpdatePriority(int priority) { updatePriority_ = priority; }
    /// @brief アクティブ状態を取得
    bool IsActive() const { return isActive_; }
    /// @brief アクティブ状態を設定
    void SetActive(bool active) { isActive_ = active; }

    /// @brief 初期化処理
    void InitializeInterface(Passkey<EmptyObject>, ObjectContext *objectContext, SceneContext *sceneContext) {
        objectContext_ = objectContext;
        sceneContext_ = sceneContext;
        Initialize();
    }
    /// @brief 終了処理
    void FinalizeInterface(Passkey<EmptyObject>) { Finalize(); }
    /// @brief 更新処理
    void UpdateInterface(Passkey<EmptyObject>) { if (IsActive()) { Update(); } }

#ifdef USE_IMGUI
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowImGuiInterface(Passkey<EmptyObject>) { ShowImGui(); }
#endif
    JSON SaveToJsonInterface(Passkey<EmptyObject>) const {
        JSON json;
        json["priority"] = updatePriority_;
        json["isActive"] = isActive_;
        json["customData"] = SaveToJson();
        return json;
    }
    bool LoadFromJsonInterface(Passkey<EmptyObject>, const JSON &json) {
        updatePriority_ = json.value("priority", 1);
        isActive_ = json.value("isActive", true);
        if (json.contains("customData")) {
            return LoadFromJson(json["customData"]);
        }
        return true;
    }

protected:
    IObjectComponent(const std::string &typeName, size_t maxCount, size_t componentTypeID)
        : kComponentType_(typeName), kMaxComponentCountPerObject_(maxCount), kComponentTypeID_(componentTypeID), updatePriority_(1) {}
#define OBJECT_COMPONENT_CONSTRUCTOR(typeName, maxCount, initializeCode) \
    typeName() : IObjectComponent(#typeName, maxCount, GetComponentTypeID<typeName>()) { REGISTER_COMPONENT_OBJECT(typeName); initializeCode }

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

    /// @brief 所属オブジェクトのコンテキストを取得
    ObjectContext *GetOwnerObjectContext() const { return objectContext_; }
    /// @brief 所属オブジェクトのシーンのコンテキストを取得
    SceneContext *GetOwnerSceneContext() const { return sceneContext_; }

    /// @brief メンバー変数を追加する
    /// @param key 変数のキー
    /// @param variable メンバー変数の情報
    void AddMemberVariable(const std::string &key, const MemberVariable &variable) {
        memberVariables_.insert_or_assign(key, variable);
    }
    /// @brief メンバー変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param ptr 変数のポインタ
    template <typename T>
    void AddMemberVariable(const std::string &key, T *variable) {
        memberVariables_.insert_or_assign(key, MemberVariable{ static_cast<void *>(variable), GetValueType<T>() });
    }
    /// @brief メンバ変数の取得
    /// @param key 変数のキー
    /// @return メンバー変数の情報（存在しない場合は nullptr）
    MemberVariable *GetMemberVariable(const std::string &key) {
        auto it = memberVariables_.find(key);
        if (it != memberVariables_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    /// @brief 全てのメンバー変数の取得
    /// @return メンバー変数のマップ
    const std::unordered_map<std::string, MemberVariable> &GetAllMemberVariables() const {
        return memberVariables_;
    }
#define ADD_MEMBER_VARIABLE(var) AddMemberVariable(#var, &var)

private:
    /// @brief コンポーネントの種類名
    const std::string kComponentType_ = "IObjectComponent";
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数
    const size_t kMaxComponentCountPerObject_ = 0xFF;
    /// @brief コンポーネントの型ID
    const size_t kComponentTypeID_ = MAXSIZE_T;

    /// @brief オーナーオブジェクトのコンテキスト
    ObjectContext *objectContext_ = nullptr;
    /// @brief 所属シーンのコンテキスト
    SceneContext *sceneContext_ = nullptr;

    /// @brief 更新優先度（小さいほど先に更新される）
    int updatePriority_ = 1;
    /// @brief アクティブ状態（falseの場合はUpdateが呼ばれない）
    bool isActive_ = true;

    /// @brief メンバー変数のマップ（ImGuiなどからアクセスするための汎用的な変数格納用）
    std::unordered_map<std::string, MemberVariable> memberVariables_;
};

} // namespace KashipanEngine
