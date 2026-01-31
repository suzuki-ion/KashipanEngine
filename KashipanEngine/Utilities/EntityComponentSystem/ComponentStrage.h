#pragma once
#include <vector>
#include <memory>
#include <bitset>
#include "EntityDefinition.h"

using ComponentSignature = std::bitset<64>;

/// @brief コンポーネント用コンテナクラス
class ComponentStorage {
private:
    /// @brief コンポーネント配列の共通インターフェイス
    struct IComponentArray {
        virtual ~IComponentArray() = default;
        virtual void RemoveData(const Entity &entity) = 0;
        virtual const std::vector<Entity> &GetEntityList() const = 0;
        virtual size_t GetSize() const = 0;
        virtual void ReserveEntityCapacity(size_t size) = 0;
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
        /// @param component 追加するコンポーネントデータ
        void InsertData(const Entity &entity, const ComponentType &component) {
            if (entityToIndex_.size() <= entity) {
                entityToIndex_.resize(entity + 1, Entity(-1));
            }
            size_t index = size_;
            componentData_.push_back(component);
            entityToIndex_[entity] = index;
            indexToEntity_.push_back(entity);
            size_++;
        }

        /// @brief コンポーネントデータの削除
        /// @param entity 対象のエンティティ
        void RemoveData(const Entity &entity) override {
            size_t index = entityToIndex_[entity];
            size_t lastIndex = size_ - 1;
            componentData_[index] = componentData_[lastIndex];
            Entity lastEntity = indexToEntity_[lastIndex];
            entityToIndex_[lastEntity] = index;
            indexToEntity_[index] = lastEntity;
            entityToIndex_[entity] = Entity(-1);
            size_--;
        }

        /// @brief コンポーネントデータの取得
        /// @param entity 対象のエンティティ
        /// @return コンポーネントデータへのポインタ（存在しない場合はnullptr）
        ComponentType *GetData(const Entity &entity) {
            size_t index = entityToIndex_[entity];
            if (index != Entity(-1) && index < size_) {
                return &componentData_[index];
            }
            return nullptr;
        }

        /// @brief コンポーネントデータの存在チェック
        /// @param entity 対象のエンティティ
        /// @return コンポーネントデータが存在するかどうか
        bool HasData(const Entity &entity) const {
            size_t index = entityToIndex_[entity];
            return index != Entity(-1) && index < size_;
        }

        /// @brief コンポーネントを持つエンティティのリストを取得
        /// @return エンティティのリスト
        const std::vector<Entity> &GetEntityList() const override {
            return indexToEntity_;
        }

        /// @brief コンポーネントの数を取得
        size_t GetSize() const override {
            return size_;
        }

        /// @brief エンティティ容量の予約
        /// @param size 予約する容量
        void ReserveEntityCapacity(size_t size) override {
            if (entityToIndex_.size() < size) {
                entityToIndex_.resize(size, Entity(-1));
            }
            if (componentData_.capacity() < size) {
                componentData_.reserve(size);
            }
            if (indexToEntity_.capacity() < size) {
                indexToEntity_.reserve(size);
            }
        }

    private:
        std::vector<ComponentType> componentData_;
        std::vector<Entity> entityToIndex_;
        std::vector<Entity> indexToEntity_;
        size_t size_ = 0;
    };

public:
    ComponentStorage() = default;
    ~ComponentStorage() = default;

    /// @brief コンポーネントの追加
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @param component 追加するコンポーネントデータ（未指定の場合はデフォルト構築）
    /// @return 追加したコンポーネントデータへのポインタ
    template <typename ComponentType>
    ComponentType *AddComponent(const Entity &entity, const ComponentType &component = {}) {
        const size_t &typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex >= componentArrays_.size()) {
            componentArrays_.resize(typeIndex + 1);
        }
        if (!componentArrays_[typeIndex]) {
            componentArrays_[typeIndex] = std::make_unique<ComponentArray<ComponentType>>();
            componentArrays_[typeIndex]->ReserveEntityCapacity(entityCapacity_);
        }
        auto *componentArray = static_cast<ComponentArray<ComponentType> *>(componentArrays_[typeIndex].get());
        componentArray->InsertData(entity, component);
        entityCapacity_ = std::max(entityCapacity_, componentArray->GetSize());
        return componentArray->GetData(entity);
    }

    /// @brief コンポーネントの削除
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return 削除に成功したかどうか
    template <typename ComponentType>
    bool RemoveComponent(const Entity &entity) {
        const size_t &typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto *componentArray = static_cast<ComponentArray<ComponentType> *>(componentArrays_[typeIndex].get());
            componentArray->RemoveData(entity);
            return true;
        }
        return false;
    }

    /// @brief 対象のエンティティからすべてのコンポーネントを削除
    /// @param entity 対象のエンティティ
    void RemoveComponent(const Entity &entity) {
        for (auto &componentArray : componentArrays_) {
            if (componentArray) {
                componentArray->RemoveData(entity);
            }
        }
    }

    /// @brief コンポーネントの取得
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントデータへのポインタ（存在しない場合はnullptr）
    template <typename ComponentType>
    ComponentType *GetComponent(const Entity &entity) {
        const size_t &typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto *componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            return componentArray->GetData(entity);
        }
        return nullptr;
    }

    /// @brief コンポーネントの存在チェック
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントが存在するかどうか
    template <typename ComponentType>
    bool HasComponent(const Entity &entity) const {
        const size_t &typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto *componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            return componentArray->HasData(entity);
        }
        return false;
    }

    /// @brief 指定したコンポーネントを持つエンティティのリストを取得
    /// @tparam ComponentTypes コンポーネントの型リスト
    /// @return エンティティのリスト
    template <typename... ComponentTypes>
    const std::vector<Entity> &GetEntitiesWithComponents() {
        static std::vector<Entity> cachedEntityList;
        cachedEntityList.clear();
        
        // 最小のコンポーネント配列を見つける
        size_t minSize = SIZE_MAX;
        size_t minIndex = SIZE_MAX;
        (([&]() {
            const size_t &typeIndex = GetTypeIndex<ComponentTypes>();
            if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
                auto *componentArray = static_cast<ComponentArray<ComponentTypes>*>(componentArrays_[typeIndex].get());
                size_t entityCount = componentArray->GetEntityList().size();
                if (entityCount < minSize) {
                    minSize = entityCount;
                    minIndex = typeIndex;
                }
            }
            }()), ...);
        if (minIndex == SIZE_MAX) {
            return cachedEntityList;
        }
        // 最小のコンポーネント配列のエンティティを基準にチェック
        auto *minComponentArray = static_cast<IComponentArray *>(componentArrays_[minIndex].get());
        for (const auto &entity : minComponentArray->GetEntityList()) {
            bool hasAllComponents = true;
            (([&]() {
                const size_t &typeIndex = GetTypeIndex<ComponentTypes>();
                if (typeIndex != minIndex) {
                    if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
                        auto *componentArray = static_cast<ComponentArray<ComponentTypes>*>(componentArrays_[typeIndex].get());
                        if (!componentArray->HasData(entity)) {
                            hasAllComponents = false;
                        }
                    } else {
                        hasAllComponents = false;
                    }
                }
                }()), ...);
            if (hasAllComponents) {
                cachedEntityList.push_back(entity);
            }
        }
        return cachedEntityList;
    }

    /// @brief 指定した型のコンポーネント数を取得
    template <typename ComponentType>
    size_t GetComponentCount() const {
        const size_t &typeIndex = GetTypeIndex<ComponentType>();
        if (typeIndex < componentArrays_.size() && componentArrays_[typeIndex]) {
            auto *componentArray = static_cast<ComponentArray<ComponentType>*>(componentArrays_[typeIndex].get());
            return componentArray->GetSize();
        }
        return 0;
    }

    /// @brief エンティティ容量の予約
    /// @param capacity 予約する容量
    void ReserveEntityCapacity(size_t capacity) {
        for (auto &componentArray : componentArrays_) {
            if (componentArray) {
                componentArray->ReserveEntityCapacity(capacity);
            }
        }
        entityCapacity_ = std::max(entityCapacity_, capacity);
    }

    /// @brief すべてのコンポーネントデータをクリア
    void ClearAllComponents() {
        componentArrays_.clear();
    }

private:
    std::vector<std::unique_ptr<IComponentArray>> componentArrays_;
    size_t entityCapacity_ = 0;

    size_t componentTypeCounter_ = 0;
    template <typename T>
    const size_t &GetTypeIndex() {
        static size_t typeIndex = componentTypeCounter_++;
        return typeIndex;
    }
};
