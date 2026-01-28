#include "EntityComponentManager.h"

namespace KashipanEngine {

Entity EntityComponentManager::CreateEntity() {
    // 再利用可能なエンティティIDがあればそれを使用
    if (!freeEntityIDs_.empty()) {
        Entity entity = freeEntityIDs_.top();
        freeEntityIDs_.pop();
        entityAliveFlags_[entity] = true;
        return entity;
    }
    // 新しいエンティティIDを割り当て
    Entity entity = nextEntityID_++;
    if (entity >= entityAliveFlags_.size()) {
        entityAliveFlags_.resize(entity + 1, false);
    }
    entityAliveFlags_[entity] = true;
    return entity;
}

void EntityComponentManager::DestroyEntity(Entity entity) {
    if (entity < entityAliveFlags_.size() && entityAliveFlags_[entity]) {
        entityAliveFlags_[entity] = false;
        freeEntityIDs_.push(entity);

        // 全てのコンポーネント配列から該当エンティティのデータを削除
        for (auto& componentArray : componentArrays_) {
            if (componentArray) {
                componentArray->RemoveData(entity);
            }
        }
    }
}

bool EntityComponentManager::IsEntityAlive(Entity entity) const {
    return entity < entityAliveFlags_.size() && entityAliveFlags_[entity];
}

void EntityComponentManager::ClearAllEntities() {
    entityAliveFlags_.clear();
    while (!freeEntityIDs_.empty()) {
        freeEntityIDs_.pop();
    }
    componentArrays_.clear();
    nextEntityID_ = 0;
}

size_t EntityComponentManager::GetAliveEntityCount() const {
    return entityAliveFlags_.size() - freeEntityIDs_.size();
}

} // namespace KashipanEngine