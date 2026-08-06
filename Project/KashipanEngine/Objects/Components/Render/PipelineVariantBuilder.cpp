#include "PipelineVariantBuilder.h"
#if defined(USE_IMGUI)

#include <algorithm>
#include <imgui.h>
#include <unordered_set>
#include <vector>

#include "Graphics/Pipeline/System/ShaderModuleComposer.h"
#include "Graphics/PipelineManager.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace PipelineVariantBuilder {

bool Show(std::string &pipelineName) {
    bool applied = false;
    if (!ImGui::TreeNode(TranslationLabel("editor.pipelinevariantbuilder.builder"))) return applied;

    static int sVariantBase = 0; // 0=Object3D, 1=Object2D
    static bool sVariantToon = false;
    static int sVariantRaster = 0; // 0=Solid, 1=DoubleSidedCulling
    static int sVariantBlend = 4; // 既定はNormal
    static std::unordered_set<std::string> sSelectedModules;
    static const char *kVariantBaseNames[] = { "Object3D", "Object2D" };
    static const char *kVariantRasterNames[] = { "Solid", "DoubleSidedCulling" };
    static const char *kVariantBlendNames[] = { "Add", "Exclusion", "Multiply", "None", "Normal", "Screen", "Subtract" };

    ImGui::Combo(TranslationLabel("editor.pipelinevariantbuilder.base"), &sVariantBase, kVariantBaseNames, IM_ARRAYSIZE(kVariantBaseNames));
    if (sVariantBase == 0) {
        ImGui::Checkbox(TranslationLabel("editor.pipelinevariantbuilder.toon"), &sVariantToon);

        // シェーダーモジュール選択（Object3Dのみ。Object2Dでは対応するシェーダーが存在しない）
        ImGui::TextUnformatted(TranslationC("editor.pipelinevariantbuilder.modules"));
        ImGui::Indent();
        for (const auto &module : GetShaderModuleRegistry()) {
            bool selected = sSelectedModules.contains(module.token);
            const std::string labelKey = "editor.pipelinevariantbuilder.module_" + module.token;
            if (ImGui::Checkbox(TranslationLabel(labelKey), &selected)) {
                if (selected) sSelectedModules.insert(module.token);
                else sSelectedModules.erase(module.token);
            }
        }
        ImGui::Unindent();
    } else {
        sVariantToon = false; // Object2D向けのToonプリセットは存在しない
        sSelectedModules.clear();
    }
    ImGui::Combo(TranslationLabel("editor.pipelinevariantbuilder.raster"), &sVariantRaster, kVariantRasterNames, IM_ARRAYSIZE(kVariantRasterNames));
    ImGui::Combo(TranslationLabel("editor.pipelinevariantbuilder.blend"), &sVariantBlend, kVariantBlendNames, IM_ARRAYSIZE(kVariantBlendNames));

    // 選択されたモジュールはソートしてから連結し、選択順によらず同じ組み合わせなら常に同じ名前になるようにする
    std::vector<std::string> sortedModules(sSelectedModules.begin(), sSelectedModules.end());
    std::sort(sortedModules.begin(), sortedModules.end());

    std::string builtName = std::string(kVariantBaseNames[sVariantBase]) + (sVariantToon ? ".Toon" : "");
    for (const auto &token : sortedModules) builtName += "." + token;
    builtName += "." + std::string(kVariantRasterNames[sVariantRaster]) + ".Blend" + kVariantBlendNames[sVariantBlend];

    ImGui::TextUnformatted(builtName.c_str());
    if (ImGui::Button(TranslationLabel("editor.pipelinevariantbuilder.apply"))) {
        if (PipelineManager::TryGetOrCreatePipeline(builtName)) {
            pipelineName = builtName;
            applied = true;
        }
    }
    ImGui::TreePop();
    return applied;
}

} // namespace PipelineVariantBuilder
} // namespace KashipanEngine

#endif // USE_IMGUI
