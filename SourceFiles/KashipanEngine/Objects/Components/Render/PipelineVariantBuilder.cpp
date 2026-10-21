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
    static const char *kVariantBlendNames[] = { "Add", "Exclusion", "Multiply", "None", "Normal", "Screen", "Subtract", "Translucent" };

    ImGui::Combo(TranslationLabel("editor.pipelinevariantbuilder.base"), &sVariantBase, kVariantBaseNames, IM_ARRAYSIZE(kVariantBaseNames));

    // ベースを切り替えた際、対応しないファミリ（3D系/2D系）のモジュール選択が残らないようにする
    for (auto it = sSelectedModules.begin(); it != sSelectedModules.end(); ) {
        const ShaderModuleDefinition *def = nullptr;
        for (const auto &module : GetShaderModuleRegistry()) {
            if (module.token == *it) { def = &module; break; }
        }
        const bool keep = def && (IsTwoDimensionalSlot(def->slot) == (sVariantBase == 1));
        if (!keep) it = sSelectedModules.erase(it);
        else ++it;
    }

    if (sVariantBase == 0) {
        ImGui::Checkbox(TranslationLabel("editor.pipelinevariantbuilder.toon"), &sVariantToon);
    } else {
        sVariantToon = false; // Object2D向けのToonプリセットは存在しない
    }

    // シェーダーモジュール選択（選択中のベースに対応するモジュールのみ表示）
    ImGui::TextUnformatted(TranslationC("editor.pipelinevariantbuilder.modules"));
    ImGui::Indent();
    for (const auto &module : GetShaderModuleRegistry()) {
        if (IsTwoDimensionalSlot(module.slot) != (sVariantBase == 1)) continue;
        bool selected = sSelectedModules.contains(module.token);
        const std::string labelKey = "editor.pipelinevariantbuilder.module_" + module.token;
        if (ImGui::Checkbox(TranslationLabel(labelKey), &selected)) {
            if (selected) sSelectedModules.insert(module.token);
            else sSelectedModules.erase(module.token);
        }
    }
    ImGui::Unindent();
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
