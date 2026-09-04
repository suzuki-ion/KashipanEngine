#pragma once
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <memory>
#include <string>

#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/ScreenBuffer.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 画面全体を数フレームにわたる指数移動平均でならす、汎用の軽量な時間的ブレンド・ポストエフェクト
/// @details モーションベクター・リプロジェクション・ディスオクルージョン処理は持たない簡易実装で、
///          いわゆる本来のTAA（カメラジッター＋モーションベクター＋リプロジェクション＋近傍クランプ）
///          とは別物（単純な時間的ブレンド/フレーム蓄積）。半透明のディザリングパターンのちらつきを
///          ならす用途で導入したが、それに限らずフレーム間でちらつく表現全般（例: ノイズベースの
///          演出、低サンプル数のスクリーンスペース効果など）を滑らかにする目的で汎用的に使える。
///          画面全体に対して一様にブレンドするため、対象以外の動く物体・カメラ操作時にも同じ強さで
///          残像（ゴースト）が乗る点はトレードオフとして許容している。
///          ヒストリーを2枚（ping-pong）持ち、毎フレーム「現在のシーン色＋前フレームのヒストリー」を
///          ブレンドして新しいヒストリーへ描き、それをオーナーの書き込み面へコピーする2パス構成
///          （1パスで完結させようとすると同じバッファへの読み取りと書き込みが同時に必要になり不可能なため）。
///          中間レンダーターゲットを使う多段パスのため、宣言的なパス定義ではなくカスタム描画で実装している
///          （MotionBlurEffect/BloomEffectと同様の構成）。
class TemporalBlendEffect final : public IPostProcessComponent {
public:
    struct Params {
        /// @brief ヒストリー（過去フレーム）をどれだけ強く残すか（0=ブレンドなし、1に近いほど強く滑らかになる
        ///        代わりに動く物体の残像も強くなる）
        float historyWeight = 0.5f;
    };

    TemporalBlendEffect() : IPostProcessComponent("TemporalBlendEffect", GetComponentTypeID<TemporalBlendEffect>()) {
        ADD_MEMBER_VARIABLE(params_.historyWeight);
    }
    ~TemporalBlendEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<TemporalBlendEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        LogScope scope;
        IPostProcessComponent::Finalize();
        history_[0] = {};
        history_[1] = {};
        blendConstantBuffer_.reset();
        writeIndex_ = 0;
        hasHistory_ = false;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.temporalblendeffect.history_weight"), &params_.historyWeight, 0.01f, 0.0f, 0.98f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.temporalblendeffect.desc_1"));
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["historyWeight"] = params_.historyWeight;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.historyWeight = json.value("historyWeight", 0.5f);
        return true;
    }

    /// @brief カスタム描画のみで完結するため宣言的なパスは返さない
    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        LogScope scope;
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        static const char *kBlend = "PostEffect.TemporalBlend";
        static const char *kCopy = "PostEffect.TemporalBlend.Copy";
        if (!context.pipelineManager->HasPipeline(kBlend) || !context.pipelineManager->HasPipeline(kCopy)) {
            return false;
        }

        if (!EnsureHistoryTargets(screenBuffer)) return false;

        // シーン色を読み取り可能にし、最終出力用の書き込み面を開く
        screenBuffer->NextPass();

        const int writeIdx = writeIndex_;
        const int readIdx = 1 - writeIndex_;
        EffectRT &dst = history_[writeIdx];
        EffectRT &prev = history_[readIdx];

        // ブレンドパス: 現在のシーン色と前フレームのヒストリーを混ぜ、新しいヒストリーへ描く。
        // 初回（まだ有効なヒストリーが無い）はhistoryWeightを0扱いにして現在の色をそのまま使う
        {
            BlendConstants cbData{};
            cbData.historyWeight = hasHistory_ ? params_.historyWeight : 0.0f;
            if (!blendConstantBuffer_) blendConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(BlendConstants));
            if (void *mapped = blendConstantBuffer_->Map()) {
                std::memcpy(mapped, &cbData, sizeof(cbData));
            }

            dst.rt->SetCommandList(commandList);
            if (!dst.rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return false;

            const auto rtv = dst.rt->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            D3D12_VIEWPORT vp{};
            vp.Width = static_cast<float>(dst.width);
            vp.Height = static_cast<float>(dst.height);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            D3D12_RECT sc{};
            sc.right = static_cast<LONG>(dst.width);
            sc.bottom = static_cast<LONG>(dst.height);
            commandList->RSSetViewports(1, &vp);
            commandList->RSSetScissorRects(1, &sc);

            context.pipelineBinder->UsePipeline(kBlend);
            auto &binder = context.getShaderBinder(kBlend);
            binder.Bind("Pixel:TemporalBlendCB", blendConstantBuffer_.get());
            binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
            binder.Bind("Pixel:gHistoryTexture", prev.srv->GetGPUDescriptorHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            commandList->DrawInstanced(3, 1, 0, 0);

            dst.rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        // コピー・パス: ブレンド結果（新しいヒストリー）をそのままオーナーの書き込み面へ出力する
        screenBuffer->RebindWriteTarget();
        {
            context.pipelineBinder->UsePipeline(kCopy);
            auto &binder = context.getShaderBinder(kCopy);
            binder.Bind("Pixel:gSceneTexture", dst.srv->GetGPUDescriptorHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            commandList->DrawInstanced(3, 1, 0, 0);
        }

        writeIndex_ = readIdx;
        hasHistory_ = true;

        return true;
    }

private:
    /// @brief TemporalBlendCB（TemporalBlendCB.hlsli）と同レイアウトの構造体
    struct BlendConstants {
        float historyWeight = 0.0f;
        float pad[3]{};
    };

    /// @brief ヒストリー1枚分のレンダーターゲット＋SRV
    struct EffectRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    /// @brief ヒストリー用レンダーターゲット（ping-pong 2枚）をオーナーのサイズ・フォーマットに合わせて用意する
    bool EnsureHistoryTargets(ScreenBuffer *owner) {
        LogScope scope;
        const std::uint32_t width = owner->GetWidth();
        const std::uint32_t height = owner->GetHeight();
        if (width == 0 || height == 0) return false;

        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (auto *rt = owner->GetRenderTarget()) {
            format = rt->GetFormat();
        }

        const bool needsRecreate = !history_[0].rt || !history_[1].rt ||
            history_[0].width != width || history_[0].height != height;
        if (needsRecreate) {
            for (auto &h : history_) {
                h.rt = std::make_unique<RenderTargetResource>(width, height, format);
                h.srv = std::make_unique<ShaderResourceResource>(h.rt.get());
                h.width = width;
                h.height = height;
            }
            // サイズが変わった以上、以前のヒストリー内容は無効（解像度不一致でサンプルできない）ため、
            // 次フレームは「ヒストリー無し」として現在の色をそのまま使うところからやり直す
            writeIndex_ = 0;
            hasHistory_ = false;
        }

        return history_[0].rt && history_[0].srv && history_[1].rt && history_[1].srv;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> blendConstantBuffer_;
    EffectRT history_[2];
    int writeIndex_ = 0;
    bool hasHistory_ = false;
};

REGISTER_COMPONENT_OBJECT(TemporalBlendEffect)

} // namespace KashipanEngine
