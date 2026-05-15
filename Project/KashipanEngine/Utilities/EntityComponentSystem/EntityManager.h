#pragma once
#include <vector>
#include <stack>
#include "EntityDefinition.h"

namespace KashipanEngine {

/// @brief エンティティ管理クラス
class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() = default;

    /// @brief エンティティが有効かどうかをチェック
    /// @param entity チェックするエンティティ
    bool IsEntityAlive(const Entity &entity) const {
        return entity.id < entityAliveFlags_.size() &&
            entityAliveFlags_[entity.id] &&
            entityGenerations_[entity.id] == entity.generation;
    }

    /// @brief エンティティ(ID)の作成
    Entity CreateEntity() {
        Entity entity;
        if (!freeEntityIDs_.empty()) {
            // 再利用可能なエンティティIDがあればそれを使用
            entity.id = freeEntityIDs_.top();
            entity.generation = entityGenerations_[entity.id];
            freeEntityIDs_.pop();
        } else {
            // 新しいエンティティIDを割り当て
            entity.id = nextEntityID_++;
            entity.generation = 0;
            if (entity.id >= entityAliveFlags_.size()) {
                entityAliveFlags_.resize(entity.id + 1, 0);
            }
            if (entity.id >= entityGenerations_.size()) {
                entityGenerations_.resize(entity.id + 1, 0);
            }
        }
        entityAliveFlags_[entity.id] = 1;
        entityGenerations_[entity.id] = entity.generation;
        return entity;
    }

    /// @brief エンティティ(ID)の削除
    /// @param entity 削除するエンティティ
    void DestroyEntity(const Entity &entity) {
        if (IsEntityAlive(entity)) {
            entityAliveFlags_[entity.id] = false;
            entityGenerations_[entity.id] = entity.generation + 1;
            freeEntityIDs_.push(entity.id);
        }
    }

    /// @brief すべてのエンティティをクリア
    void ClearAllEntities() {
        nextEntityID_ = 0;
        entityAliveFlags_.clear();
        entityGenerations_.clear();
        while (!freeEntityIDs_.empty()) {
            freeEntityIDs_.pop();
        }
    }

    /// @brief 有効なエンティティ数を取得
    size_t GetAliveEntityCount() const {
        size_t count = 0;
        for (const auto &flag : entityAliveFlags_) {
            if (flag) {
                count++;
            }
        }
        return count;
    }

    /// @brief エンティティ容量の予約
    /// @param capacity 予約する容量
    void ReserveEntityCapacity(size_t capacity) {
        if (capacity > entityAliveFlags_.size()) {
            entityAliveFlags_.resize(capacity, 0);
        }
        if (capacity > entityGenerations_.size()) {
            entityGenerations_.resize(capacity, 0);
        }
    }

private:
    size_t nextEntityID_ = 0;
    std::vector<char> entityAliveFlags_;
    std::vector<uint32_t> entityGenerations_;
    std::stack<size_t> freeEntityIDs_;
};

} // namespace KashipanEngine