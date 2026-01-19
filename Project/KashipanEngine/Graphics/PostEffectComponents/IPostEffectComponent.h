#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <optional>
#include <vector>

#include "Graphics/Renderer.h"

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

    /// @brief このコンポーネントが提供するポストエフェクトパス一覧を返す
    /// @details 返り値はフレーム毎に再構築してよい（軽量な前提）。
    virtual std::vector<PostEffectPass> BuildPostEffectPasses() const { return {}; }

#if defined(USE_IMGUI)
    /// @brief ポストエフェクトのパラメータ表示/調整UI（Begin/Endは呼ばない）
    virtual void ShowImGui() {}
#endif

    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IPostEffectComponent> Clone() const = 0;

protected:
    IPostEffectComponent(const std::string& componentType, size_t maxComponentCountPerBuffer)
        : kComponentType_(componentType), kMaxComponentCountPerBuffer_(maxComponentCountPerBuffer) {}

    /// @brief 所属 ScreenBuffer の取得
    ScreenBuffer* GetOwnerBuffer() const { return ownerBuffer_; }

    static RenderCommand MakeDrawCommand(UINT vertexCount, UINT startVertexLocation = 0) {
        RenderCommand cmd;
        cmd.vertexCount = vertexCount;
        cmd.indexCount = 0;
        cmd.startVertexLocation = startVertexLocation;
        return cmd;
    }

    static RenderCommand MakeDrawIndexedCommand(UINT indexCount, UINT startIndexLocation = 0, INT baseVertexLocation = 0) {
        RenderCommand cmd;
        cmd.vertexCount = 0;
        cmd.indexCount = indexCount;
        cmd.startIndexLocation = startIndexLocation;
        cmd.baseVertexLocation = baseVertexLocation;
        return cmd;
    }

private:
    friend class ScreenBuffer;

    void SetOwnerBuffer(ScreenBuffer* buffer) {
        if (!buffer) assert(false && "Owner buffer cannot be null.");
        ownerBuffer_ = buffer;
    }

    const std::string kComponentType_ = "IPostEffectComponent";
    const size_t kMaxComponentCountPerBuffer_ = 0xFF;

    ScreenBuffer* ownerBuffer_ = nullptr;
};

} // namespace KashipanEngine
