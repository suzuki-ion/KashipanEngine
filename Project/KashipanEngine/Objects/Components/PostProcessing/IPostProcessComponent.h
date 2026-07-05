#pragma once
#include <d3d12.h>
#include <functional>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Assets/SamplerManager.h"

namespace KashipanEngine {

class Renderer;

/// @brief ポストエフェクト用オブジェクトコンポーネントの基底クラス
/// @details ScreenBufferObject コンポーネントが付与されたオブジェクトへ付与することで、
///          そのスクリーンバッファに対してポストエフェクトがかけられる。
///          オブジェクトへは IPostProcessComponent として登録されるように、
///          コンポーネントIDは IPostProcessComponent から生成したIDで固定にしている。
class IPostProcessComponent : public IObjectComponent {
public:
    struct PassInfo {
        struct ConstantBufferRequirement {
            std::string variableName;
            size_t byteSize = 0;
            void *dataPtr = nullptr;
        };
        struct InstanceBufferRequirement {
            std::string variableName;
            size_t elementStride = 0;
            void *dataPtr = nullptr;
        };
        /// @brief 追加テクスチャのバインド要求（ノイズテクスチャや深度テクスチャ等）
        struct TextureBindRequirement {
            std::string variableName;
            /// @brief バインド時に呼ばれるSRVハンドル取得関数（無効ハンドルを返した場合はスキップ）
            std::function<D3D12_GPU_DESCRIPTOR_HANDLE()> getHandle;
        };
        /// @brief サンプラーのバインド要求
        struct SamplerBindRequirement {
            std::string variableName;
            DefaultSampler sampler = DefaultSampler::LinearClamp;
        };

        std::string pipelineName;
        std::vector<ConstantBufferRequirement> constantBufferRequirements;
        std::vector<InstanceBufferRequirement> instanceBufferRequirements;
        std::vector<TextureBindRequirement> textureBindRequirements;
        std::vector<SamplerBindRequirement> samplerBindRequirements;
    };
    virtual ~IPostProcessComponent() = default;

    /// @brief ポストエフェクトパス情報の構築（Renderer から呼ばれる）
    std::vector<PassInfo> BuildPassesInterface(Passkey<Renderer>) { return BuildPasses(); }

protected:
    IPostProcessComponent(const std::string &componentType, size_t maxComponentCountPerBuffer = 0xFF)
        : IObjectComponent(componentType, maxComponentCountPerBuffer, GetComponentTypeID<IPostProcessComponent>()) {}

    /// @brief ポストエフェクトパス情報を構築する（派生クラスで実装）
    /// @return 実行するパスのリスト（1コンポーネントで複数パスを返してもよい）
    virtual std::vector<PassInfo> BuildPasses() = 0;

    /// @brief 同一オブジェクトの ScreenBufferObject からスクリーンバッファを取得
    ScreenBuffer *GetOwnerScreenBuffer() const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return nullptr;
        auto *screenBufferObject = objectContext->GetComponent<ScreenBufferObject>();
        return screenBufferObject ? screenBufferObject->GetScreenBuffer() : nullptr;
    }
};

} // namespace KashipanEngine
