#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <optional>

#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

namespace KashipanEngine {

class ScreenBuffer;

/// @brief ScreenBuffer 向けポストエフェクトコンポーネントのインターフェース
class IPostEffectComponent {
public:
    virtual ~IPostEffectComponent() = default;
    IPostEffectComponent(const IPostEffectComponent&) = delete;
    IPostEffectComponent& operator=(const IPostEffectComponent&) = delete;
    IPostEffectComponent(IPostEffectComponent&&) = delete;
    IPostEffectComponent& operator=(IPostEffectComponent&&) = delete;

    /// @brief コンポーネントの種類を取得
    const std::string& GetComponentType() const { return kComponentType_; }
    /// @brief 1つの ScreenBuffer に登録可能な同じコンポーネントの最大数を取得
    size_t GetMaxComponentCountPerBuffer() const { return kMaxComponentCountPerBuffer_; }

    /// @brief 初期化処理（ScreenBuffer に登録された直後に呼ばれる想定）
    virtual std::optional<bool> Initialize() { return std::nullopt; }
    /// @brief 終了処理（ScreenBuffer から削除される直前に呼ばれる想定）
    virtual std::optional<bool> Finalize() { return std::nullopt; }

    /// @brief ポストエフェクト適用（ScreenBuffer の描画結果に対して実行）
    virtual std::optional<bool> Apply() { return std::nullopt; }

    /// @brief シェーダー変数へのバインド処理
    virtual std::optional<bool> BindShaderVariables(ShaderVariableBinder* binder) {
        (void)binder;
        return std::nullopt;
    }

    /// @brief Apply の処理優先順位（小さいほど先に処理される）
    int GetApplyPriority() const { return applyPriority_; }
    void SetApplyPriority(int priority) { applyPriority_ = priority; }

    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IPostEffectComponent> Clone() const = 0;

protected:
    IPostEffectComponent(const std::string& componentType, size_t maxComponentCountPerBuffer)
        : kComponentType_(componentType), kMaxComponentCountPerBuffer_(maxComponentCountPerBuffer) {}

    /// @brief 所属 ScreenBuffer の取得
    ScreenBuffer* GetOwnerBuffer() const { return ownerBuffer_; }

private:
    friend class ScreenBuffer;

    void SetOwnerBuffer(ScreenBuffer* buffer) {
        if (!buffer) assert(false && "Owner buffer cannot be null.");
        ownerBuffer_ = buffer;
    }

    const std::string kComponentType_ = "IPostEffectComponent";
    const size_t kMaxComponentCountPerBuffer_ = 0xFF;

    int applyPriority_ = 1;

    ScreenBuffer* ownerBuffer_ = nullptr;
};

} // namespace KashipanEngine
