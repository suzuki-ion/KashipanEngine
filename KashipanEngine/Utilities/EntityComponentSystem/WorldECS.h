#pragma once
#include <vector>
#include <stack>
#include <memory>
#include "EntityDefinition.h"
#include "EntityManager.h"
#include "ComponentStrage.h"
#include "SystemManager.h"

namespace KashipanEngine {

/// @brief ECSワールドクラス
class WorldECS {
public:
    WorldECS() = default;
    ~WorldECS() = default;

    /// @brief エンティティの作成
    /// @return 作成したエンティティ
    Entity CreateEntity() {
        return entityManager_.CreateEntity();
    }

    /// @brief エンティティの削除
    /// @param entity 削除するエンティティ
    void DestroyEntity(const Entity &entity) {
        entityManager_.DestroyEntity(entity);
        componentStorage_.RemoveComponent(entity);
    }

    /// @brief エンティティが有効かどうかをチェック
    /// @param entity チェックするエンティティ
    /// @return 有効かどうか
    bool IsEntityAlive(const Entity &entity) const {
        return entityManager_.IsEntityAlive(entity);
    }

    /// @brief 有効なエンティティ数を取得
    size_t GetAliveEntityCount() const {
        return entityManager_.GetAliveEntityCount();
    }

    /// @brief コンポーネントの追加
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @param component 追加するコンポーネントデータ（未指定の場合はデフォルト構築）
    /// @return 追加したコンポーネントデータへのポインタ
    template <typename ComponentType>
    ComponentType *AddComponent(const Entity &entity, const ComponentType &component = {}) {
        return componentStorage_.AddComponent<ComponentType>(entity, component);
    }
    
    /// @brief コンポーネントの削除
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    template <typename ComponentType>
    void RemoveComponent(const Entity &entity) {
        componentStorage_.RemoveComponent<ComponentType>(entity);
    }

    /// @brief 対象のエンティティからすべてのコンポーネントを削除
    /// @param entity 対象のエンティティ
    void RemoveComponent(const Entity &entity) {
        componentStorage_.RemoveComponent(entity);
    }

    /// @brief コンポーネントの取得
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントデータへのポインタ（存在しない場合はnullptr）
    template <typename ComponentType>
    ComponentType *GetComponent(const Entity &entity) {
        return componentStorage_.GetComponent<ComponentType>(entity);
    }

    /// @brief コンポーネントの存在チェック
    /// @tparam ComponentType コンポーネントの型
    /// @param entity 対象のエンティティ
    /// @return コンポーネントが存在するかどうか
    template <typename ComponentType>
    bool HasComponent(const Entity &entity) const {
        return componentStorage_.HasComponent<ComponentType>(entity);
    }

    /// @brief 指定したコンポーネントを持つエンティティのリストを取得
    /// @tparam ComponentTypes コンポーネントの型リスト
    /// @return エンティティのリスト
    template <typename... ComponentTypes>
    const std::vector<Entity> &GetEntitiesWithComponents() {
        return componentStorage_.GetEntitiesWithComponents<ComponentTypes...>();
    }

    /// @brief システムの追加
    /// @tparam SystemType システムの型
    /// @param priority システムの優先度
    /// @param args システムのコンストラクタ引数
    /// @return 追加したシステムへのポインタ
    template <typename SystemType, typename... Args>
    SystemType *AddSystem(size_t priority = 0, Args&&... args) {
        return systemManager_.AddSystem<SystemType>(priority, std::forward<Args>(args)...);
    }

    /// @brief システムの削除
    /// @tparam SystemType システムの型
    /// @return 削除に成功したかどうか
    template <typename SystemType>
    bool RemoveSystem() {
        return systemManager_.RemoveSystem<SystemType>();
    }

    /// @brief システムの取得
    /// @tparam SystemType システムの型
    /// @return システムへのポインタ（存在しない場合はnullptr）
    template <typename SystemType>
    SystemType* GetSystem() {
        return systemManager_.GetSystem<SystemType>();
    }

    /// @brief システムの存在チェック
    /// @tparam SystemType システムの型
    /// @return システムが存在するかどうか
    template <typename SystemType>
    bool HasSystem() const {
        return systemManager_.HasSystem<SystemType>();
    }

    /// @brief すべてのシステムを更新
    /// @param deltaTime 経過時間
    void UpdateAllSystems(float deltaTime) {
        systemManager_.UpdateAllSystems(componentStorage_, deltaTime);
    }

    /// @brief エンティティ容量の予約
    /// @param capacity 予約する容量
    void ReserveEntityCapacity(size_t capacity) {
        entityManager_.ReserveEntityCapacity(capacity);
        componentStorage_.ReserveEntityCapacity(capacity);
    }

    /// @brief すべてのエンティティとコンポーネント、システムをクリア
    void ClearAll() {
        entityManager_.ClearAllEntities();
        componentStorage_.ClearAllComponents();
        systemManager_.ClearAllSystems();
    }

private:
    EntityManager entityManager_;
    ComponentStorage componentStorage_;
    SystemManager systemManager_;
};

} // namespace KashipanEngine