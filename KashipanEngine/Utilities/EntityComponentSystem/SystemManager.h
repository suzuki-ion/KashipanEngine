#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "EntityDefinition.h"
#include "ComponentStrage.h"

namespace KashipanEngine {

/// @brief システムの共通インターフェイス
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(ComponentStorage &componentStorage, float deltaTime) = 0;
};

/// @brief 指定したコンポーネントを持つエンティティごとの処理を行う基底 System
template <typename... ComponentTypes>
class ComponentSystem : public ISystem {
public:
    ComponentSystem() = default;
    ~ComponentSystem() override = default;

    /// @brief エンティティごとの更新処理
    /// @param componentStorage コンポーネントストレージ
    /// @param deltaTime 経過時間
    void Update(ComponentStorage &componentStorage, float deltaTime) override {
        const auto &entities = componentStorage.GetEntitiesWithComponents<ComponentTypes...>();
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
            UpdateEntity(entities[i], componentStorage, deltaTime);
        }
    }

protected:
    // 1 エンティティ分の処理を派生クラスで実装する
    virtual void UpdateEntity(const Entity &entity, ComponentStorage &componentStorage, float deltaTime) = 0;
};

/// @brief システムマネージャークラス
class SystemManager {
private:
    /// @brief システムエントリ構造体
    struct SystemEntry {
        size_t priority = 0;
        std::unique_ptr<ISystem> system = nullptr;
    };

public:
    SystemManager() = default;
    ~SystemManager() = default;
    
    /// @brief システムの追加
    /// @tparam SystemType システムの型
    /// @param args システムのコンストラクタ引数
    /// @return 追加したシステムへのポインタ
    template <typename SystemType, typename... Args>
    SystemType *AddSystem(size_t priority = 0, Args&&... args) {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        if (systemIndex >= systemEntries_.size()) {
            systemEntries_.resize(systemIndex + 1);
        }
        auto systemEntry = std::make_unique<SystemEntry>();
        systemEntry->priority = priority;
        systemEntry->system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        SystemType* systemPtr = static_cast<SystemType*>(systemEntry->system.get());
        systemEntries_[systemIndex] = std::move(systemEntry);
        isDirty_ = true;
        return systemPtr;
    }

    /// @brief システムの削除
    /// @tparam SystemType システムの型
    /// @return 削除に成功したかどうか
    template <typename SystemType>
    bool RemoveSystem() {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        if (systemIndex < systemEntries_.size() && systemEntries_[systemIndex]) {
            systemEntries_[systemIndex].reset();
            isDirty_ = true;
            return true;
        }
        return false;
    }

    /// @brief システムの取得
    /// @tparam SystemType システムの型
    /// @return システムへのポインタ（存在しない場合はnullptr）
    template <typename SystemType>
    SystemType* GetSystem() {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        if (systemIndex < systemEntries_.size() && systemEntries_[systemIndex]) {
            return static_cast<SystemType*>(systemEntries_[systemIndex]->system.get());
        }
        return nullptr;
    }

    /// @brief システムの存在チェック
    /// @tparam SystemType システムの型
    /// @return システムが存在するかどうか
    template <typename SystemType>
    bool HasSystem() const {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        return systemIndex < systemEntries_.size() && systemEntries_[systemIndex];
    }

    /// @brief システムの優先度設定
    /// @tparam SystemType システムの型
    /// @param priority 優先度
    template <typename SystemType>
    void SetSystemPriority(size_t priority) {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        if (systemIndex < systemEntries_.size() && systemEntries_[systemIndex]) {
            systemEntries_[systemIndex]->priority = priority;
            isDirty_ = true;
        }
    }

    /// @brief システムの優先度取得
    /// @tparam SystemType システムの型
    /// @return 優先度（存在しない場合は-1）
    template <typename SystemType>
    int GetSystemPriority() const {
        const size_t &systemIndex = GetSystemIndex<SystemType>();
        if (systemIndex < systemEntries_.size() && systemEntries_[systemIndex]) {
            return static_cast<int>(systemEntries_[systemIndex]->priority);
        }
        return -1;
    }

    /// @brief すべてのシステムを更新
    /// @param componentStorage コンポーネントストレージ
    /// @param deltaTime 経過時間
    void UpdateAllSystems(ComponentStorage &componentStorage, float deltaTime) {
        static std::vector<SystemEntry *> sortedSystems;
        if (isDirty_) {
            sortedSystems.clear();
            for (const auto &entry : systemEntries_) {
                if (entry) {
                    sortedSystems.push_back(entry.get());
                }
            }
            std::sort(sortedSystems.begin(), sortedSystems.end(),
                      [](const SystemEntry *a, const SystemEntry *b) {
                          return a->priority < b->priority;
                      });
            isDirty_ = false;
        }
        for (int i = 0; i < static_cast<int>(sortedSystems.size()); ++i) {
            sortedSystems[i]->system->Update(componentStorage, deltaTime);
        }
    }

    /// @brief すべてのシステムをクリア
    void ClearAllSystems() {
        systemEntries_.clear();
    }

private:
    std::vector<std::unique_ptr<SystemEntry>> systemEntries_;
    bool isDirty_ = false;

    size_t systemIDCounter_ = 0;
    template <typename SystemType>
    const size_t &GetSystemIndex() {
        static size_t systemIndex = systemIDCounter_++;
        return systemIndex;
    }
};

} // namespace KashipanEngine