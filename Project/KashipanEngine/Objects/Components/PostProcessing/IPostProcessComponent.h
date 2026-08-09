#pragma once
#include <d3d12.h>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Assets/SamplerManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Scene/SceneContext.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class Renderer;
class PipelineManager;
class PipelineBinder;
class ShaderVariableBinder;

/// @brief ポストエフェクト用オブジェクトコンポーネントの基底クラス
/// @details ScreenBufferObject コンポーネントが付与されたオブジェクトへ付与することで、
///          そのスクリーンバッファに対してポストエフェクトがかけられる。
///          派生クラスは各々が個別のコンポーネント型（GetComponentTypeID<派生型>()）として
///          登録される（コンストラクタでコンポーネントIDを指定すること）。Rendererから
///          一括で「そのオブジェクトのポストエフェクト一覧」を取得できるよう、
///          Initialize/FinalizeでSceneRendererへ自身を登録/解除する
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

    /// @brief このスクリーンバッファへ実際に描画したカメラの情報
    /// @details 深度バッファからワールド座標を再構成したい（AO等）、あるいは深度の線形化に
    ///          Near/Farが必要（Outline等）ポストエフェクトが、ユーザーにカメラ設定の手動複製を
    ///          要求せずに済むよう Renderer が解決して注入する。該当するカメラが見つからなかった
    ///          場合は valid=false（この場合、RenderCustom側は何もせず false を返し、
    ///          BuildPasses側は近似できないことを踏まえた既定値で処理すること）。
    struct CameraInfo {
        bool valid = false;
        Matrix4x4 viewProjection = Matrix4x4::Identity();
        Vector3 worldPosition{ 0.0f, 0.0f, 0.0f };
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };

    virtual ~IPostProcessComponent() = default;

    /// @brief カメラ情報の注入（Renderer から BuildPasses/RenderCustom より前に毎フレーム呼ばれる）
    void SetCameraInfoInterface(Passkey<Renderer>, const CameraInfo &info) { cameraInfo_ = info; }

    /// @brief ポストエフェクトパス情報の構築（Renderer から呼ばれる）
    std::vector<PassInfo> BuildPassesInterface(Passkey<Renderer>) { return BuildPasses(); }

    /// @brief カスタム描画（Renderer から呼ばれる）
    /// @return 描画を行った場合は true（その場合 BuildPasses による描画は行われない）
    bool RenderCustomInterface(Passkey<Renderer>, CustomRenderContext &context) { return RenderCustom(context); }

    /// @brief 指定のスクリーンバッファがこのポストエフェクトの適用対象に含まれるか（除外設定されていないか）
    /// @details 同一オブジェクトに複数の ScreenBufferObject が付与されている場合に、
    ///          どのスクリーンバッファへ適用するかを個別に除外設定できる。
    bool IsScreenBufferIncluded(const ScreenBuffer *screenBuffer) const {
        if (!screenBuffer) return false;
        return !excludedScreenBufferNames_.contains(screenBuffer->GetRenderTargetName());
    }

protected:
    /// @param componentType コンポーネントの種類名（派生クラス名の文字列）
    /// @param componentTypeID 派生クラス自身の型ID（派生クラスのコンストラクタで
    ///        `GetComponentTypeID<派生型>()` を渡すこと。これにより各エフェクトが
    ///        個別のコンポーネント型として `GetComponent<派生型>()` 等で取得できるようになる）
    /// @param maxComponentCountPerBuffer 1オブジェクトに付与できる最大数
    IPostProcessComponent(const std::string &componentType, size_t componentTypeID, size_t maxComponentCountPerBuffer = 0xFF)
        : IObjectComponent(componentType, maxComponentCountPerBuffer, componentTypeID) {}

    /// @brief SceneRendererへ自身を登録する（派生クラスがoverrideする場合は必ず
    ///        `IPostProcessComponent::Initialize();` を呼ぶこと）
    void Initialize() override {
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) sceneRenderer->RegisterPostProcessComponent(this);
    }

    /// @brief SceneRendererから自身の登録を解除する（派生クラスがoverrideする場合は必ず
    ///        `IPostProcessComponent::Finalize();` を呼ぶこと）
    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) sceneRenderer->UnregisterPostProcessComponent(this);
    }

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

    /// @brief Rendererが注入した、このスクリーンバッファへ描画したカメラの情報を取得する
    const CameraInfo &GetCameraInfo() const noexcept { return cameraInfo_; }

#if defined(USE_IMGUI)
    /// @brief 適用先ScreenBufferObjectの除外設定UI（派生クラスは自身のShowImGui()の先頭でこれを呼ぶこと）
    /// @details 同一オブジェクトが持つ ScreenBufferObject が2つ以上ある場合のみ表示する
    void ShowImGui() override {
        auto *objectContext = GetOwnerObjectContext();
        auto screenBufferObjects = objectContext ? objectContext->GetComponents<ScreenBufferObject>() : std::vector<ScreenBufferObject *>{};
        if (screenBufferObjects.size() <= 1) return;

        if (ImGui::TreeNode(TranslationLabel("component.ipostprocesscomponent.apply_target_filter"))) {
            for (auto *screenBufferObject : screenBufferObjects) {
                auto *buffer = screenBufferObject ? screenBufferObject->GetScreenBuffer() : nullptr;
                if (!buffer) continue;
                const std::string &name = buffer->GetRenderTargetName();
                bool included = !excludedScreenBufferNames_.contains(name);
                ImGui::PushID(buffer);
                if (ImGui::Checkbox(name.c_str(), &included)) {
                    if (included) {
                        excludedScreenBufferNames_.erase(name);
                    } else {
                        excludedScreenBufferNames_.insert(name);
                    }
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
#endif

    /// @brief 派生クラスは自身のSaveToJson()の先頭で `JSON json = IPostProcessComponent::SaveToJson();` として呼び、
    ///        追加のフィールドをマージすること
    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["excludedScreenBuffers"] = JSON::array();
        for (const auto &name : excludedScreenBufferNames_) {
            json["excludedScreenBuffers"].push_back(name);
        }
        return json;
    }

    /// @brief 派生クラスは自身のLoadFromJson()の先頭で `IPostProcessComponent::LoadFromJson(json);` を呼ぶこと
    bool LoadFromJson(const JSON &json) override {
        excludedScreenBufferNames_.clear();
        for (const auto &name : json.value("excludedScreenBuffers", std::vector<std::string>())) {
            excludedScreenBufferNames_.insert(name);
        }
        return true;
    }

private:
    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    /// @brief 適用対象から除外するスクリーンバッファ名（GetRenderTargetName()）の集合
    std::unordered_set<std::string> excludedScreenBufferNames_;
    /// @brief Rendererから注入された、このスクリーンバッファへ描画したカメラの情報
    CameraInfo cameraInfo_;
};

} // namespace KashipanEngine
