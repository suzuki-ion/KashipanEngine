#include "ComputeShaderProcessing.h"

#include "Debug/Logger.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <imgui.h>

#include "Assets/TextureManager.h"
#include "Graphics/PipelineManager.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

#if defined(USE_IMGUI)
void ComputeShaderProcessing::ShowImGui() {
    LogScope scope;
    // 使用するComputeパイプラインは読み込み済みのものから選択する
    ImGuiCustom::SelectString(TranslationLabel("component.computeshaderprocessing.pipeline"), pipelineName_, PipelineManager::GetLoadedComputePipelineNames());

    int gx = static_cast<int>(groupCountX_);
    int gy = static_cast<int>(groupCountY_);
    int gz = static_cast<int>(groupCountZ_);
    if (ImGuiCustom::EditValue(TranslationLabel("component.computeshaderprocessing.groupcountx"), gx)) groupCountX_ = static_cast<std::uint32_t>(std::max(1, gx));
    if (ImGuiCustom::EditValue(TranslationLabel("component.computeshaderprocessing.groupcounty"), gy)) groupCountY_ = static_cast<std::uint32_t>(std::max(1, gy));
    if (ImGuiCustom::EditValue(TranslationLabel("component.computeshaderprocessing.groupcountz"), gz)) groupCountZ_ = static_cast<std::uint32_t>(std::max(1, gz));

    //--------- 読み取り専用テクスチャ ---------//
    if (ImGui::TreeNode(TranslationLabel("component.computeshaderprocessing.textures_srv"))) {
        std::vector<std::string> texturePaths;
        for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
            texturePaths.push_back(entry.assetPath);
        }
        int removeIndex = -1;
        for (size_t i = 0; i < textureBindRequirements_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &req = textureBindRequirements_[i];
            ImGui::InputText(TranslationLabel("component.computeshaderprocessing.variable"), &req.variableName);
            ImGuiCustom::SelectString(TranslationLabel("component.computeshaderprocessing.texture"), req.textureAssetPath, texturePaths, true);
            if (ImGui::SmallButton(TranslationLabel("component.computeshaderprocessing.remove"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            textureBindRequirements_.erase(textureBindRequirements_.begin() + removeIndex);
        }
        if (ImGui::Button(TranslationLabel("component.computeshaderprocessing.add_texture_bind"))) {
            textureBindRequirements_.push_back({});
        }
        ImGui::TreePop();
    }

    //--------- 読み書き可能なUAVテクスチャ ---------//
    if (ImGui::TreeNode(TranslationLabel("component.computeshaderprocessing.uav_textures"))) {
        static const char *kFormatNames[] = { "RGBA8_UNORM", "R32_FLOAT", "RGBA32_FLOAT" };
        int removeIndex = -1;
        for (size_t i = 0; i < uavTextureBindRequirements_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &req = uavTextureBindRequirements_[i];
            ImGui::InputText(TranslationLabel("component.computeshaderprocessing.variable"), &req.variableName);
            int w = static_cast<int>(req.width);
            int h = static_cast<int>(req.height);
            if (ImGuiCustom::EditValue(TranslationLabel("component.computeshaderprocessing.width"), w)) req.width = static_cast<std::uint32_t>(std::max(1, w));
            if (ImGuiCustom::EditValue(TranslationLabel("component.computeshaderprocessing.height"), h)) req.height = static_cast<std::uint32_t>(std::max(1, h));
            ImGui::Combo(TranslationLabel("component.computeshaderprocessing.format"), &req.formatKind, kFormatNames, IM_ARRAYSIZE(kFormatNames));
            if (ImGui::SmallButton(TranslationLabel("component.computeshaderprocessing.remove"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            uavTextureBindRequirements_.erase(uavTextureBindRequirements_.begin() + removeIndex);
        }
        if (ImGui::Button(TranslationLabel("component.computeshaderprocessing.add_uav_texture_bind"))) {
            uavTextureBindRequirements_.push_back({});
        }
        ImGui::TreePop();
    }

    //--------- 定数バッファ（float配列） ---------//
    if (ImGui::TreeNode(TranslationLabel("component.computeshaderprocessing.constant_buffers"))) {
        int removeIndex = -1;
        for (size_t i = 0; i < constantBufferBindRequirements_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &req = constantBufferBindRequirements_[i];
            ImGui::InputText(TranslationLabel("component.computeshaderprocessing.variable"), &req.variableName);
            int removeValueIndex = -1;
            for (size_t v = 0; v < req.values.size(); ++v) {
                ImGui::PushID(static_cast<int>(v));
                ImGui::DragFloat("##Value", &req.values[v], 0.01f);
                ImGui::SameLine();
                if (ImGui::SmallButton("-")) removeValueIndex = static_cast<int>(v);
                ImGui::PopID();
            }
            if (removeValueIndex >= 0) {
                req.values.erase(req.values.begin() + removeValueIndex);
            }
            if (ImGui::SmallButton(TranslationLabel("component.computeshaderprocessing.add_value"))) req.values.push_back(0.0f);
            ImGui::SameLine();
            if (ImGui::SmallButton(TranslationLabel("component.computeshaderprocessing.remove_buffer"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            constantBufferBindRequirements_.erase(constantBufferBindRequirements_.begin() + removeIndex);
        }
        if (ImGui::Button(TranslationLabel("component.computeshaderprocessing.add_constant_buffer_bind"))) {
            constantBufferBindRequirements_.push_back({});
        }
        ImGui::TreePop();
    }
}
#endif

JSON ComputeShaderProcessing::SaveToJson() const {
    LogScope scope;
    JSON json = JSON::object();
    json["pipelineName"] = pipelineName_;
    json["groupCountX"] = groupCountX_;
    json["groupCountY"] = groupCountY_;
    json["groupCountZ"] = groupCountZ_;

    for (const auto &req : textureBindRequirements_) {
        JSON reqJson = JSON::object();
        reqJson["variableName"] = req.variableName;
        reqJson["textureAssetPath"] = req.textureAssetPath;
        json["textureBinds"].push_back(reqJson);
    }
    for (const auto &req : uavTextureBindRequirements_) {
        JSON reqJson = JSON::object();
        reqJson["variableName"] = req.variableName;
        reqJson["width"] = req.width;
        reqJson["height"] = req.height;
        reqJson["formatKind"] = req.formatKind;
        json["uavTextureBinds"].push_back(reqJson);
    }
    for (const auto &req : constantBufferBindRequirements_) {
        JSON reqJson = JSON::object();
        reqJson["variableName"] = req.variableName;
        reqJson["values"] = req.values;
        json["constantBufferBinds"].push_back(reqJson);
    }
    return json;
}

bool ComputeShaderProcessing::LoadFromJson(const JSON &json) {
    LogScope scope;
    pipelineName_ = json.value("pipelineName", std::string{});
    groupCountX_ = json.value("groupCountX", 1u);
    groupCountY_ = json.value("groupCountY", 1u);
    groupCountZ_ = json.value("groupCountZ", 1u);

    textureBindRequirements_.clear();
    for (const auto &reqJson : json.value("textureBinds", std::vector<JSON>())) {
        TextureBindRequirement req;
        req.variableName = reqJson.value("variableName", std::string{});
        req.textureAssetPath = reqJson.value("textureAssetPath", std::string{});
        textureBindRequirements_.push_back(std::move(req));
    }
    uavTextureBindRequirements_.clear();
    for (const auto &reqJson : json.value("uavTextureBinds", std::vector<JSON>())) {
        UAVTextureBindRequirement req;
        req.variableName = reqJson.value("variableName", std::string{});
        req.width = reqJson.value("width", 256u);
        req.height = reqJson.value("height", 256u);
        req.formatKind = reqJson.value("formatKind", 0);
        uavTextureBindRequirements_.push_back(std::move(req));
    }
    constantBufferBindRequirements_.clear();
    for (const auto &reqJson : json.value("constantBufferBinds", std::vector<JSON>())) {
        ConstantBufferBindRequirement req;
        req.variableName = reqJson.value("variableName", std::string{});
        req.values = reqJson.value("values", std::vector<float>());
        constantBufferBindRequirements_.push_back(std::move(req));
    }
    return true;
}

} // namespace KashipanEngine
