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
class PipelineManager;
class PipelineBinder;
class ShaderVariableBinder;

/// @brief ポストエフェクト用オブジェクトコンポーネントの基底クラス
/// @details ScreenBufferObject コンポーネントが付与されたオブジェクトへ付与することで、
///          そのスクリーンバッファに対してポストエフェクトがかけられる。
///          オブジェクトへは IPostProcessComponent として登録されるように、
///          コンポーネントIDは IPostProcessComponent から生成したIDで固定にしている。
class IPostProcessComponent : public IObjectComponent {
public:
    // 派生エフェクトは全て PostProcessing カテゴリに分類される
    COMPONENT_CATEGORY("PostProcessing")

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
    /// @brief カスタム描画用コンテキスト（Renderer から渡される）
    struct CustomRenderContext {
        ScreenBuffer *screenBuffer = nullptr;
        ID3D12GraphicsCommandList *commandList = nullptr;
        PipelineManager *pipelineManager = nullptr;
        PipelineBinder *pipelineBinder = nullptr;
        /// @brief パイプライン名からシェーダー変数バインダーを取得する関数
        std::function<ShaderVariableBinder &(const std::string &)> getShaderBinder;
    };

    virtual ~IPostProcessComponent() = default;

    /// @brief ポストエフェクトパス情報の構築（Renderer から呼ばれる）
    std::vector<PassInfo> BuildPassesInterface(Passkey<Renderer>) { return BuildPasses(); }

    /// @brief カスタム描画（Renderer から呼ばれる）
    /// @return 描画を行った場合は true（その場合 BuildPasses による描画は行われない）
    bool RenderCustomInterface(Passkey<Renderer>, CustomRenderContext &context) { return RenderCustom(context); }

protected:
    IPostProcessComponent(const std::string &componentType, size_t maxComponentCountPerBuffer = 0xFF)
        : IObjectComponent(componentType, maxComponentCountPerBuffer, GetComponentTypeID<IPostProcessComponent>()) {}

    /// @brief ポストエフェクトパス情報を構築する（派生クラスで実装）
    /// @return 実行するパスのリスト（1コンポーネントで複数パスを返してもよい）
    virtual std::vector<PassInfo> BuildPasses() = 0;

    /// @brief 宣言的なパス定義で表現できないエフェクト用のカスタム描画（必要な派生クラスで実装）
    /// @details 中間レンダーターゲットを使う多段パス等はこちらで実装する。
    ///          描画は screenBuffer のコマンドリストに記録し、最後の書き込みは
    ///          screenBuffer->NextPass() 後の書き込み面（RebindWriteTarget で再設定可能）へ行うこと。
    /// @return 描画を行った場合は true
    virtual bool RenderCustom(CustomRenderContext &) { return false; }

    /// @brief 同一オブジェクトの ScreenBufferObject からスクリーンバッファを取得
    ScreenBuffer *GetOwnerScreenBuffer() const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return nullptr;
        auto *screenBufferObject = objectContext->GetComponent<ScreenBufferObject>();
        return screenBufferObject ? screenBufferObject->GetScreenBuffer() : nullptr;
    }
};

} // namespace KashipanEngine
