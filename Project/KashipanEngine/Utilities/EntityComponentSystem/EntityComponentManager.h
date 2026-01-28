#pragma once
#include <vector>
#include <stack>
#include <unordered_map>
#include <memory>
#include <span>

namespace KashipanEngine {

using Entity = size_t;

/// @brief エンティティ・コンポーネント管理クラス
class EntityComponentManager {
private:
    /// @brief コンポーネント配列の共通インターフェイス
    struct IComponentArray {
        virtual ~IComponentArray() = default;
        virtual void RemoveData(Entity entity) = 0;
        virtual const std::vector<Entity>& GetEntityList() const = 0;
    };

    /// @brief コンポーネント配列クラス
    /// @tparam ComponentType コンポーネントの型
    template <typename ComponentType>
    class ComponentArray : public IComponentArray {
    public:
        ComponentArray() = default;
        ~ComponentArray() = default;

        /// @brief コンポーネントデータの挿入
        /// @param entity 対象のエンティティ
        /// @param component 挿入するコンポーネントデータ
        void InsertData(Entity entity, ComponentType component) {
            if (entity >= componentData_.size()) {
                componentData_.resize(entity + 1);
                hasComponent_.resize(entity + 1, false);
                entityIndex_.resize(entity + 1, npos);
            }
            if (!hasComponent_[entity]) {
                entities_.push_back(entity);
                entityIndex_[entity] = entities_.size() - 1;
            }
            componentData_[entity] = component;
            hasComponent_[entity] = true;
        }
        /// @brief コンポーネントデータの削除
        /// @param entity 対象のエンティティ
        void RemoveData(Entity entity) override {
            if (entity < hasComponent_.size() && hasComponent_[entity]) {
                hasComponent_[entity] = false;
                size_t idx = entityIndex_[entity];
                if (idx != npos) {
                    size_t lastIdx = entities_.size() - 1;
                    if (idx != lastIdx) {
                        Entity swapped = entities_[lastIdx];
                        entities_[idx] = swapped;
                        entityIndex_[swapped] = idx;
                    }
                    entities_.pop_back();
                    entityIndex_[entity] = npos;
                }
            }
        }
        /// @brief コンポーネントデータの取得
        /// @param entity 対象のエンティティ
        /// @return コンポーネントデータへのポインタ（存在しない場合はnullptr）
        ComponentType* GetData(Entity entity) {
            if (entity < hasComponent_.size() && hasComponent_[entity]) {
                return &componentData_[entity];
            }
            return nullptr;
        }
        /// @brief コンポーネントデータの存在チェック
        /// @param entity 対象のエンティティ
        bool HasData(Entity entity) const {
            return entity < hasComponent_.size() && hasComponent_[entity];
        }

        const std::vector<Entity>& GetEntityList() const override { return entities_; }

    private:
        std::vector<ComponentType> componentData_;
        std::vector<char> hasComponent_;
        std::vector<Entity> entities_;
        std::vector<size_t> entityIndex_;
        static constexpr size_t npos = static_cast<size_t>(-1);
    };

public:
    EntityComponentManager() = default;
    ~EntityComponentManager() = default;

    /// @brief エンティティ(ID)の作成
    Entity CreateEntity();
    /// @brief エンティティ(ID)の削除
    /// @param entity 削除するエンティティ
    void DestroyEntity(Entity entity);
    /// @brief エンティティが有効かどうかをチェック
    /// @param entity チェックするエンティティ
    bool IsEntityAlive(Entity entity) const;
    /// @brief すべてのエンティティをクリア
    void ClearAllEntities();
    /// @brief 有効なエンティティ数を取得
    size_t GetAliveEntityCount() const;

    /// @brief コンポーネントの追加
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @param component 追加するコンポーネントデータ
    template <typename ComponentType>
    void AddComponent(Entity entity, ComponentType component = {}) {
        size_t typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex >= componentArrays_.size()) {
            componentArrays_.resize(typeIndex + 1);
        }
        if (!componentArrays_[typeIndex]) {
            componentArrays_[typeIndex] = std::make_unique<ComponentArray<ComponentType>>();
        }
        auto* componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
        componentArray->InsertData(entity, component);
    }
    /// @brief コンポーネントの削除
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    template <typename ComponentType>
    void RemoveComponent(Entity entity) {
        size_t typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto* componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            componentArray->RemoveData(entity);
        }
    }
    /// @brief コンポーネントの取得
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントデータへのポインタ（存在しない場合はnullptr）
    template <typename ComponentType>
    ComponentType* GetComponent(Entity entity) {
        size_t typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto* componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            return componentArray->GetData(entity);
        }
        return nullptr;
    }
    /// @brief コンポーネントの存在チェック
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントが存在するかどうか
    template <typename ComponentType>
    bool HasComponent(Entity entity) const {
        size_t typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto* componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            return componentArray->HasData(entity);
        }
        return false;
    }

    /// @brief 指定のコンポーネントを持つエンティティのリストを取得
    /// @tparam ComponentTypes コンポーネントの型リスト
    /// @return エンティティのリスト
    template <typename... ComponentTypes>
    std::vector<Entity> GetEntitiesWithComponents() {
        std::vector<Entity> result;
        std::vector<const std::vector<Entity>*> lists;
        lists.reserve(sizeof...(ComponentTypes));
        bool anyMissing = false;
        ([&]() {
            size_t typeIndex = GetTypeIndex<ComponentTypes>();
            if (typeIndex >= componentArrays_.size() || !componentArrays_[typeIndex]) {
                anyMissing = true;
                return;
            }
            auto* arr = static_cast<ComponentArray<ComponentTypes>*>(componentArrays_[typeIndex].get());
            lists.push_back(&arr->GetEntityList());
        }(), ...);
        if (anyMissing || lists.empty()) return result;
        const std::vector<Entity>* smallest = lists[0];
        for (auto ptr : lists) {
            if (ptr->size() < smallest->size()) smallest = ptr;
        }
        for (Entity e : *smallest) {
            bool ok = true;
            bool checks[] = { (GetComponent<ComponentTypes>(e) != nullptr)... };
            for (bool c : checks) { if (!c) { ok = false; break; } }
            if (ok) result.push_back(e);
        }
        return result;
    }

private:
    /// @brief エンティティの生存フラグ
    std::vector<bool> entityAliveFlags_;
    /// @brief 再利用可能なエンティティIDのスタック
    std::stack<Entity> freeEntityIDs_;
    /// @brief 次に割り当てるエンティティID
    Entity nextEntityID_ = 0;

    /// @brief コンポーネント配列
    std::vector<std::unique_ptr<IComponentArray>> componentArrays_;

    /// @brief コンポーネント型インデックス取得用
    static inline size_t sTypeIndexCounter_ = 0;
    template <typename T>
    static size_t GetTypeIndex() {
        static size_t typeIndex = sTypeIndexCounter_++;
        return typeIndex;
    }
};

} // namespace KashipanEngine