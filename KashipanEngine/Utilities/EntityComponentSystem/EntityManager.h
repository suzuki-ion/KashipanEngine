#pragma once
#include <vector>
#include <stack>
#include "EntityDefinition.h"

/// @brief エンティティ管理クラス
class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() = default;

    /// @brief エンティティ(ID)の作成
    Entity CreateEntity() {
        Entity entity;
        if (!freeEntityIDs_.empty()) {
            // 再利用可能なエンティティIDがあればそれを使用
            entity = freeEntityIDs_.top();
            freeEntityIDs_.pop();
        } else {
            // 新しいエンティティIDを割り当て
            entity = nextEntityID_++;
            if (entity >= entityAliveFlags_.size()) {
                entityAliveFlags_.resize(entity + 1, 0);
            }
        }
        entityAliveFlags_[entity] = 1;
        return entity;
    }

    /// @brief エンティティ(ID)の削除
    /// @param entity 削除するエンティティ
    void DestroyEntity(const Entity &entity) {
        if (entity < entityAliveFlags_.size() && entityAliveFlags_[entity]) {
            entityAliveFlags_[entity] = false;
            freeEntityIDs_.push(entity);
        }
    }

    /// @brief エンティティが有効かどうかをチェック
    /// @param entity チェックするエンティティ
    bool IsEntityAlive(const Entity &entity) const {
        return entity < entityAliveFlags_.size() && entityAliveFlags_[entity];
    }

    /// @brief すべてのエンティティをクリア
    void ClearAllEntities() {
        nextEntityID_ = 0;
        entityAliveFlags_.clear();
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
    }

private:
    size_t nextEntityID_ = 0;
    std::vector<char> entityAliveFlags_;
    std::stack<Entity> freeEntityIDs_;
};