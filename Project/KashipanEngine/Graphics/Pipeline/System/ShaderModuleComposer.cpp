#include "ShaderModuleComposer.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Debug/Logger.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {
namespace {

std::string ReadFileText(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WriteFileText(const std::string &path, const std::string &content) {
    EnsureParentDirectoryExists(path);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file << content;
    return true;
}

// マーカー文字列。Object/ObjectPS.hlsl内の対応するコメントと完全に一致させること
constexpr const char *kAdditionalMaterialFieldsMarker = "/*{{ADDITIONAL_MATERIAL_FIELDS}}*/";
constexpr const char *kModuleLogicMarker = "/*{{MODULE_LOGIC}}*/";
constexpr const char *kToneHookBegin = "/*{{TONE_HOOK_BEGIN}}*/";
constexpr const char *kToneHookEnd = "/*{{TONE_HOOK_END}}*/";
constexpr const char *kRimColorHookBegin = "/*{{RIM_COLOR_HOOK_BEGIN}}*/";
constexpr const char *kRimColorHookEnd = "/*{{RIM_COLOR_HOOK_END}}*/";
constexpr const char *kDirectionalHooksMarker = "/*{{DIRECTIONAL_HOOKS}}*/";
constexpr const char *kCompositeHooksMarker = "/*{{COMPOSITE_HOOKS}}*/";
constexpr const char *kAlphaHooksMarker = "/*{{ALPHA_HOOKS}}*/";

// beginマーカーからendマーカーまで（両端含む）を置換する。見つからなければ何もしない
void ReplaceRegion(std::string &source, const std::string &beginMarker, const std::string &endMarker, const std::string &replacement) {
    size_t begin = source.find(beginMarker);
    if (begin == std::string::npos) return;
    size_t end = source.find(endMarker, begin);
    if (end == std::string::npos) return;
    end += endMarker.size();
    source.replace(begin, end - begin, replacement);
}

// マーカー文字列そのものを置換する。見つからなければ何もしない
void ReplaceMarker(std::string &source, const std::string &marker, const std::string &replacement) {
    size_t pos = source.find(marker);
    if (pos == std::string::npos) return;
    source.replace(pos, marker.size(), replacement);
}

const ShaderModuleDefinition *FindModule(const std::vector<ShaderModuleDefinition> &registry, const std::string &token) {
    for (const auto &m : registry) {
        if (m.token == token) return &m;
    }
    return nullptr;
}

} // namespace

const std::vector<ShaderModuleDefinition> &GetShaderModuleRegistry() {
    static const std::vector<ShaderModuleDefinition> kModules = {
        { "MultiTone",    ModuleHookSlot::Tone,        0,   "return ApplyMultiTone(halfLambert, mat);" },
        { "RimShade",     ModuleHookSlot::RimColor,    0,   "float3 rimColor = ApplyRimShade(mat, lam);" },
        { "Backlight",    ModuleHookSlot::Directional, 10,  "ApplyBacklight(lightingColor, mat, shadingNormal, viewDir, toLightDir, light, shadow);" },
        { "Matcap",       ModuleHookSlot::Composite,   10,  "ApplyMatcap(output.color, mat, shadingNormal);" },
        { "Gradation",    ModuleHookSlot::Composite,   20,  "ApplyGradation(output.color, mat, toonFactor);" },
        { "Emission",     ModuleHookSlot::Composite,   30,  "ApplyEmission(output.color, mat, transformedUV.xy);" },
        { "ColorGrading", ModuleHookSlot::Composite,   100, "ApplyColorGrading(output.color, mat);" },
        { "DistanceFade", ModuleHookSlot::Alpha,       10,  "ApplyDistanceFade(output.color, mat, input.worldPosition);" },
        { "Dissolve",     ModuleHookSlot::Alpha,       20,  "ApplyDissolve(output.color, mat, input.worldPosition);" },
    };
    return kModules;
}

std::string ComposeAndWriteShader(const std::vector<std::string> &selectedTokens, const std::string &shaderBaseDir) {
    if (selectedTokens.empty()) return {};

    const std::string basePath = shaderBaseDir + "/Object/ObjectPS.hlsl";
    std::string source = ReadFileText(basePath);
    if (source.empty()) {
        Log("[ShaderModuleComposer] failed to read base shader: " + basePath, LogSeverity::Error);
        return {};
    }

    const auto &registry = GetShaderModuleRegistry();

    // 選択トークンをソートしておく（生成ファイル名を選択順によらず安定させるため）
    std::vector<std::string> sortedTokens = selectedTokens;
    std::sort(sortedTokens.begin(), sortedTokens.end());

    std::string additionalFields;
    std::string moduleLogic;
    std::vector<const ShaderModuleDefinition *> directionalHooks;
    std::vector<const ShaderModuleDefinition *> compositeHooks;
    std::vector<const ShaderModuleDefinition *> alphaHooks;
    const ShaderModuleDefinition *toneModule = nullptr;
    const ShaderModuleDefinition *rimColorModule = nullptr;
    std::string combinedName;

    for (const auto &token : sortedTokens) {
        const auto *def = FindModule(registry, token);
        if (!def) continue; // 未登録トークンは無視する

        additionalFields += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Fields.hlsli") + "\n";
        moduleLogic += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Logic.hlsli") + "\n";
        if (!combinedName.empty()) combinedName += ".";
        combinedName += token;

        switch (def->slot) {
        case ModuleHookSlot::Tone: toneModule = def; break;
        case ModuleHookSlot::RimColor: rimColorModule = def; break;
        case ModuleHookSlot::Directional: directionalHooks.push_back(def); break;
        case ModuleHookSlot::Composite: compositeHooks.push_back(def); break;
        case ModuleHookSlot::Alpha: alphaHooks.push_back(def); break;
        }
    }

    if (combinedName.empty()) return {}; // 有効なモジュールが1つも無かった

    auto byPriority = [](const ShaderModuleDefinition *a, const ShaderModuleDefinition *b) { return a->priority < b->priority; };
    std::sort(directionalHooks.begin(), directionalHooks.end(), byPriority);
    std::sort(compositeHooks.begin(), compositeHooks.end(), byPriority);
    std::sort(alphaHooks.begin(), alphaHooks.end(), byPriority);

    auto joinCalls = [](const std::vector<const ShaderModuleDefinition *> &hooks) {
        std::string result;
        for (const auto *h : hooks) result += h->callExpression + "\n";
        return result;
    };

    ReplaceMarker(source, kAdditionalMaterialFieldsMarker, additionalFields);
    ReplaceMarker(source, kModuleLogicMarker, moduleLogic);
    if (toneModule) ReplaceRegion(source, kToneHookBegin, kToneHookEnd, toneModule->callExpression);
    if (rimColorModule) ReplaceRegion(source, kRimColorHookBegin, kRimColorHookEnd, rimColorModule->callExpression);
    ReplaceMarker(source, kDirectionalHooksMarker, joinCalls(directionalHooks));
    ReplaceMarker(source, kCompositeHooksMarker, joinCalls(compositeHooks));
    ReplaceMarker(source, kAlphaHooksMarker, joinCalls(alphaHooks));

    // Object/ObjectPS.hlslと同じディレクトリへ書き出す。DXCの既定のインクルードハンドラは#pragma onceを
    // インクルードパスの文字列一致で判定しており、ディレクトリを分けると相対インクルード（"../Common/..."等）が
    // 経路によって異なる文字列に展開され、同じファイル（例: Camera3D.hlsli）が二重定義されるエラーになる
    // （実際に発生させて確認済み）。同じディレクトリに置けば、コピー元と全く同じ文字列で解決されるため安全
    const std::string outputPath = shaderBaseDir + "/Object/Generated.Object3D.Compose." + combinedName + ".hlsl";
    if (!WriteFileText(outputPath, source)) {
        Log("[ShaderModuleComposer] failed to write generated shader: " + outputPath, LogSeverity::Error);
        return {};
    }
    return outputPath;
}

} // namespace KashipanEngine
