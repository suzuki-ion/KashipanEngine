#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Transform.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Scene/Components/Render/SceneRenderer.h"

namespace KashipanEngine {

/// @brief 描画時に使用するライトへ付与するコンポーネント
/// @details 同一オブジェクトの Transform と Light からライト情報を計算して
///          定数バッファへアップロードする。
///          使用するパイプラインと、そのパイプラインに含まれるシェーダーの
///          どの定数バッファへバインドするかを指定できる。
class LightRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(LightRenderer, 1, SetUpdatePriority(950);)
    ~LightRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<LightRenderer>();
        ptr->pipelineName_ = pipelineName_;
        ptr->bindVariableNames_ = bindVariableNames_;
        return ptr;
    }

    /// @brief 使用するパイプラインを設定（空文字の場合は全パイプラインに適用）
    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    /// @brief バインド先の定数バッファ変数名を設定（例: "Pixel:gDirectionalLight"）
    void SetBindVariableNames(const std::vector<std::string> &names) { bindVariableNames_ = names; }
    const std::vector<std::string> &GetBindVariableNames() const noexcept { return bindVariableNames_; }

    /// @brief ライト情報の定数バッファを取得
    ConstantBufferResource *GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

protected:
    void Initialize() override {
        if (!constantBuffer_) {
            constantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DirectionalLightConstant));
        }
        if (bindVariableNames_.empty()) {
            bindVariableNames_ = { "Pixel:gDirectionalLight" };
        }
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterLightRenderer(this);
        }
        UploadLightConstant();
    }

    void Update() override {
        UploadLightConstant();
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterLightRenderer(this);
        }
        constantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::InputText("Pipeline", &pipelineName_);
        for (const auto &name : bindVariableNames_) {
            ImGui::BulletText("%s", name.c_str());
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["pipelineName"] = pipelineName_;
        json["bindVariableNames"] = bindVariableNames_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        pipelineName_ = json.value("pipelineName", std::string{});
        bindVariableNames_.clear();
        if (json.contains("bindVariableNames")) {
            for (const auto &name : json["bindVariableNames"]) {
                bindVariableNames_.push_back(name.get<std::string>());
            }
        }
        return true;
    }

private:
    /// @brief gDirectionalLight 定数バッファと同レイアウトの構造体
    struct DirectionalLightConstant {
        std::uint32_t enabled = 0;
        float padding[3]{};
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 direction{ 0.0f, -1.0f, 0.0f };
        float intensity = 1.0f;
    };

    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    void UploadLightConstant() {
        if (!constantBuffer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        DirectionalLightConstant constant{};
        constant.enabled = IsActive() ? 1u : 0u;

        if (auto *light = objectContext->GetComponent<Light>()) {
            constant.color = light->GetColor();
            constant.intensity = light->GetIntensity();
            constant.enabled = (light->IsActive() && IsActive()) ? 1u : 0u;
        }

        // Transform の前方ベクトル（+Z）をライト方向とする
        if (auto *transform = objectContext->GetComponent<Transform>()) {
            const Matrix4x4 &world = transform->GetWorldMatrix();
            Vector3 forward(world.m[2][0], world.m[2][1], world.m[2][2]);
            const float length = forward.Length();
            if (length > 0.0f) {
                constant.direction = forward * (1.0f / length);
            }
        }

        void *mapped = constantBuffer_->Map();
        if (!mapped) return;
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    std::unique_ptr<ConstantBufferResource> constantBuffer_;
    std::string pipelineName_;
    std::vector<std::string> bindVariableNames_;
};

REGISTER_COMPONENT_OBJECT(LightRenderer)

} // namespace KashipanEngine
