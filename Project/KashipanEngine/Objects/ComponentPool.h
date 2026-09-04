#pragma once
#include <type_traits>
#include "Debug/Logger.h"
#include "Objects/ChunkedPool.h"

namespace KashipanEngine {

class IObjectComponent;

/// @brief コンポーネント型ごとのプールを型消去して扱うための基底インターフェース
class IComponentPoolBase {
public:
    virtual ~IComponentPoolBase() = default;

    /// @brief デフォルト構築でコンポーネントを追加
    /// @return 追加されたコンポーネントへのポインタ
    virtual IObjectComponent *EmplaceDefault() = 0;
    /// @brief コンポーネントを破棄し、スロットを再利用可能にする
    virtual bool Remove(const IObjectComponent *component) = 0;
    /// @brief このプールが指定ポインタのコンポーネントを現在所有しているか
    virtual bool Owns(const IObjectComponent *component) const = 0;
};

/// @brief 具体的なコンポーネント型 T 専用のプール
/// @details 内部ストレージは ChunkedPool<T> で、要素は絶対に再配置されない。
template <typename T>
class ComponentPool final : public IComponentPoolBase {
    static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

public:
    IObjectComponent *EmplaceDefault() override { LogScope scope; return pool_.EmplaceDefault(); }

    /// @brief 引数を転送して直接配置構築する（コンパイル時に型Tが分かっている呼び出し元専用）
    template <typename... Args>
    T *Emplace(Args &&...args) { LogScope scope; return pool_.Emplace(std::forward<Args>(args)...); }

    bool Remove(const IObjectComponent *component) override {
        LogScope scope;
        return pool_.Remove(static_cast<const T *>(component));
    }

    bool Owns(const IObjectComponent *component) const override {
        LogScope scope;
        return pool_.Owns(static_cast<const T *>(component));
    }

private:
    ChunkedPool<T> pool_;
};

} // namespace KashipanEngine
