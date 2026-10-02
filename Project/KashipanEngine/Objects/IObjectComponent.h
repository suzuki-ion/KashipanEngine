#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <typeindex>
#include "Utilities/FileIO.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/ComponentRef.h"
#include "Utilities/MyAny.h"
#include "Utilities/Tag.h"
#if defined(USE_IMGUI)
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

class EmptyObject;
class ObjectContext;
class SceneContext;

/// @brief コンポーネントが型ごとの一括更新（バッチ処理）対象かどうかを判定するトレイト
/// @details クラスに public static な constexpr bool IsBatchProcessed() が定義されていればそれを使い、
///          未定義の場合はバッチ処理対象外（オブジェクト単位で個別にUpdateが呼ばれる、今まで通りの動作）として扱う。
///          バッチ処理対象とマークされた型は EmptyObject::RegenerateUpdateComponentsList() で
///          個別Update呼び出しの対象から除外される。ただし現時点ではこの仕組みの土台のみで、
///          Scene側で型ごとに一括更新を回す実装は存在しない（どの型もまだこのフラグをtrueにしていない）。
template <typename T, typename = void>
struct ComponentBatchTraits {
    static constexpr bool kIsBatchProcessed = false;
};
template <typename T>
struct ComponentBatchTraits<T, std::void_t<decltype(T::IsBatchProcessed())>> {
    static constexpr bool kIsBatchProcessed = T::IsBatchProcessed();
};

/// @brief オブジェクトコンポーネントインターフェースクラス
/// @details 派生クラスは COMPONENT_CATEGORY マクロ（または public static な
///          std::vector<std::string> GetComponentCategory()）でカテゴリを宣言できる。
///          カテゴリは階層構造（例: {"Collision", "Collider"}）で、
///          Add Component メニューにてカテゴリごとのツリーで表示される。
class IObjectComponent {
    /// @brief コンポーネントの型ID設定用
    static inline size_t sComponentTypeID = 0;
public:
    /// @brief 公開するのは型と書き込み可否だけ。値はコンポーネントの型付きAPIを経由する。
    class MemberVariable {
    public:
        const TypeInfo &GetTypeInfo() const noexcept { return typeInfo_; }
        bool IsWritable() const noexcept { return static_cast<bool>(write_); }
    private:
        friend class IObjectComponent;
        MemberVariable(const void *value, TypeInfo typeInfo, std::type_index cppType,
            std::function<bool(const void *)> write)
            : value_(value), typeInfo_(std::move(typeInfo)), cppType_(cppType), write_(std::move(write)) {}
        const void *value_;
        TypeInfo typeInfo_;
        std::type_index cppType_;
        std::function<bool(const void *)> write_;
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
    /// @details EmptyObject/ObjectContext の完全な型定義が必要なため、定義は IObjectComponent.cpp にある
    bool IsActive() const;
    /// @brief アクティブ状態を設定
    /// @details オブジェクトへ登録済みの場合、有効化時にInitialize、無効化時にFinalizeが走る。
    ///          定義は EmptyObject/ObjectContext の完全な型定義が必要なため IObjectComponent.cpp にある
    void SetActive(bool active);

    /// @brief タグを設定する（文字列からハッシュ化される。空文字で未設定に戻る）
    void SetTag(const std::string &tagName) {
        tagName_ = tagName;
        tag_ = Tag(tagName);
    }
    /// @brief タグを取得する（比較用）
    const Tag &GetTag() const noexcept { return tag_; }
    /// @brief タグの文字列を取得する（表示・保存用）
    const std::string &GetTagName() const noexcept { return tagName_; }

    /// @brief 所属オブジェクトを取得する（未所属の場合は nullptr）
    /// @details ObjectContext の完全な型定義が必要なため、定義は IObjectComponent.cpp にある
    const EmptyObject *GetOwnerObject() const;
    /// @brief このコンポーネントが所属オブジェクトのコンポーネント一覧へ登録済みかを取得する
    /// @details AddComponent() 内部では、プール上のインスタンスへ状態を転送するために
    ///          登録前の LoadFromJson() が一度呼ばれる。その段階でウィンドウ等の外部リソースを
    ///          生成しないための判定に使用する。
    bool IsRegisteredToOwner() const;
    /// @brief このコンポーネント自身を指す ComponentRef を取得する
    /// @details フレームをまたいで安全に保持するためのハンドル。プールのスロット再利用による
    ///          エイリアシングを避けるため、生ポインタの代わりにこちらを保持し、使う直前に
    ///          SceneContext::ResolveComponent() 等で毎回解決すること。
    ///          定義は EmptyObject の完全な型定義が必要なため IObjectComponent.cpp にある
    ComponentRef GetComponentRef() const;

    /// @brief 初期化処理
    /// @details コンテキストは常に設定されるが、Initialize はコンポーネントが
    ///          アクティブかつ allowInitialize が true の場合のみ実行される。
    ///          （非アクティブの場合は有効化時に Initialize が走る）
    /// @param allowInitialize 所有オブジェクトが非アクティブの場合などに false を渡す
    void InitializeInterface(Passkey<EmptyObject>, ObjectContext *objectContext, SceneContext *sceneContext, bool allowInitialize = true) {
        objectContext_ = objectContext;
        sceneContext_ = sceneContext;
        if (allowInitialize && IsActive()) {
            Initialize();
        }
    }
    /// @brief 終了処理
    void FinalizeInterface(Passkey<EmptyObject>) { Finalize(); }
    /// @brief 更新処理
    void UpdateInterface(Passkey<EmptyObject>) { if (IsActive()) { Update(); } }

#ifdef USE_IMGUI
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowImGuiInterface(Passkey<EmptyObject>) { ShowImGui(); }
    /// @brief 常時ImGui表示（ゲームループがポーズ中でも毎フレーム呼ばれる）
    void ShowPersistentImGuiInterface(Passkey<EmptyObject>) { if (IsActive()) { ShowPersistentImGui(); } }
#endif
    JSON SaveToJsonInterface(Passkey<EmptyObject>) const {
        JSON json;
        json["priority"] = updatePriority_;
        json["isActive"] = isActive_;
        json["tag"] = tagName_;
        json["customData"] = SaveToJson();
        return json;
    }
    bool LoadFromJsonInterface(Passkey<EmptyObject>, const JSON &json) {
        updatePriority_ = json.value("priority", 1);
        isActive_ = json.value("isActive", true);
        SetTag(json.value("tag", std::string{}));
        if (json.contains("customData")) {
            return LoadFromJson(json["customData"]);
        }
        return true;
    }

    /// @brief メンバ変数の読み取り専用メタデータを取得する
    /// @param key 変数のキー
    /// @return メンバー変数の情報（存在しない場合は nullptr）
    const MemberVariable *GetMemberVariable(const std::string &key) const {
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

    /// @brief 実際のC++型が一致する場合だけ、値のコピーを返す
    template <typename T>
    bool GetMemberValue(const std::string &key, T &value) const {
        const auto *member = GetMemberVariable(key);
        if (!member || !member->value_ || member->cppType_ != std::type_index(typeid(T))) return false;
        value = *static_cast<const T *>(member->value_);
        return true;
    }
    /// @brief 型と書き込み権限を確認し、登録済みのセッター／通知を必ず実行する
    template <typename T>
    bool SetMemberValue(const std::string &key, const T &value) {
        const auto *member = GetMemberVariable(key);
        if (!member || !member->write_ || member->cppType_ != std::type_index(typeid(T))) return false;
        return member->write_(&value);
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
        ImGui::Text("%s", TranslationC("component.iobjectcomponent.none"));
    }
    /// @brief 常時ImGui表示（ビューアウィンドウ等、ポーズ中も表示し続けたいものに使う）
    virtual void ShowPersistentImGui() {}
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
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param variable 変数のポインタ
    /// @param onModified SetMemberValueによる書き込み後に必ず呼ぶ通知
    template <typename T>
    void AddMemberVariable(const std::string &key, T *variable, std::function<void()> onModified = nullptr) {
        AddMemberProperty<T>(key, variable, [variable, onModified = std::move(onModified)](const T &value) {
            if (!variable) return false;
            *variable = value;
            if (onModified) onModified();
            return true;
        });
    }
    /// @brief 検証・副作用を持つ変数は専用セッターへ委譲する（拒否時はfalse）
    template <typename T>
    void AddMemberProperty(const std::string &key, const T *variable, std::function<bool(const T &)> setter) {
        memberVariables_.insert_or_assign(key, MemberVariable(variable, GetValueType<T>(), typeid(T),
            [setter = std::move(setter)](const void *value) { return setter && setter(*static_cast<const T *>(value)); }));
    }
    template <typename T>
    void AddReadOnlyMemberVariable(const std::string &key, const T *variable) {
        memberVariables_.insert_or_assign(key, MemberVariable(variable, GetValueType<T>(), typeid(T), {}));
    }
#define ADD_MEMBER_VARIABLE(var) AddMemberVariable(#var, &var)
#define ADD_MEMBER_VARIABLE_WITH_CALLBACK(var, ...) AddMemberVariable(#var, &var, __VA_ARGS__)

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
    /// @brief タグ（比較用ハッシュ）と表示・保存用のタグ文字列
    Tag tag_;
    std::string tagName_;

    /// @brief メンバー変数のマップ（ImGuiなどからアクセスするための汎用的な変数格納用）
    std::unordered_map<std::string, MemberVariable> memberVariables_;
};

} // namespace KashipanEngine
