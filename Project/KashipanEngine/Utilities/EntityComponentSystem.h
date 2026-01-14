#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

// メモ：作成途中。設計が思いつかな過ぎる。

namespace KashipanEngine {

/// @brief 簡易的なエンティティコンポーネントシステム
class EntityComponentSystem {
    /// @brief ECS用エンティティクラス
    class Entity {
        // エンティティ生成時に各項目の設定を行うためECSクラスをフレンドにする
        friend class EntityComponentSystem;
    public:
        /// @brief エンティティがアクティブかどうかを取得する
        /// @return アクティブならtrue、非アクティブならfalse
        bool IsActive() const { return isActive_; }
        /// @brief エンティティ名を取得する
        /// @return エンティティ名
        const std::string &GetName() const { return name_; }
        /// @brief エンティティIDを取得する
        /// @return エンティティID
        uint32_t GetID() const { return id_; }
        /// @brief 所持しているコンポーネントIDリストを取得する
        const std::vector<uint32_t> &GetComponentIDs() const { return componentIDs_; }
    private:
        /// @brief エンティティがアクティブかどうか
        bool isActive_ = false;
        /// @brief エンティティ名
        std::string name_;
        /// @brief 自身のID
        uint32_t id_ = 0;
        /// @brief 所持しているコンポーネントのIDリスト
        std::vector<uint32_t> componentIDs_;
    };

public:
    /// @brief コンポーネントのインターフェース
    class IComponent {
        // コンポーネント生成時に各項目の設定を行うためECSクラスをフレンドにする
        friend class EntityComponentSystem;
    public:
        virtual ~IComponent() = default;
        /// @brief 所属しているエンティティIDを取得する
        uint32_t GetOwnerEntityID() const { return ownerEntityID_; }
    private:
        /// @brief 所属しているエンティティID
        uint32_t ownerEntityID_ = 0;
    };

    /// @brief コンストラクタ
    /// @param entityCapacity エンティティの初期容量
    /// @param componentCapacity コンポーネントの初期容量
    EntityComponentSystem(uint32_t entityCapacity = 1000, uint32_t componentCapacity = 1000) {
        entities_.reserve(entityCapacity);
        components_.reserve(componentCapacity);
    }
    ~EntityComponentSystem() = default;

    /// @brief エンティティを生成する
    /// @param name エンティティ名
    Entity &CreateEntity(const std::string &name);

    /// @brief エンティティを削除する
    /// @param entityID 削除するエンティティID
    void DestroyEntity(uint32_t entityID);

    /// @brief コンポーネントを生成する
    /// @tparam T コンポーネントの型
    /// @param ownerEntityID 所属するエンティティID
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 生成したコンポーネントの参照
    template<typename T, typename... Args>
    T &CreateComponent(uint32_t ownerEntityID, Args&&... args) {
        uint32_t componentID;
        if (!componentPool_.empty()) {
            componentID = componentPool_.back();
            componentPool_.pop_back();
        } else {
            componentID = static_cast<uint32_t>(components_.size());
            components_.emplace_back(nullptr);
        }
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->ownerEntityID_ = ownerEntityID;
        components_[componentID] = std::move(component);
        // 所属エンティティにコンポーネントIDを追加
        entities_[ownerEntityID].componentIDs_.push_back(componentID);
        return *static_cast<T*>(components_[componentID].get());
    }

    /// @brief コンポーネントを削除する
    /// @param componentID 削除するコンポーネントID

private:
    /// @brief エンティティ格納用配列
    std::vector<Entity> entities_;
    /// @brief エンティティ名からIDへのマッピング
    std::unordered_map<std::string, uint32_t> entityNameToIndex_;
    /// @brief コンポーネント格納用配列
    std::vector<std::unique_ptr<IComponent>> components_;

    /// @brief 空きのエンティティIDリスト
    std::vector<uint32_t> entityPool_;
    /// @brief 空きのコンポーネントIDリスト
    std::vector<uint32_t> componentPool_;
};

} // namespace KashipanEngine