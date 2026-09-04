#include "ShaderModuleComposer.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

#include "Debug/Logger.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {
namespace {

// std::ifstream/ofstream(const std::string&)はWindows上で現在のANSIコードページを使ってファイルを
// 開くため、日本語等の非ASCII文字を含むパスが開けない。Utf8StringToPathでpath版コンストラクタへ
// 渡して回避する
std::string ReadFileText(const std::string &path) {
    LogScope scope;
    std::ifstream file(Utf8StringToPath(path), std::ios::binary);
    if (!file) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WriteFileText(const std::string &path, const std::string &content) {
    LogScope scope;
    EnsureParentDirectoryExists(path);
    std::ofstream file(Utf8StringToPath(path), std::ios::binary | std::ios::trunc);
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
constexpr const char *kPreLightingHooksMarker = "/*{{PRELIGHTING_HOOKS}}*/";
constexpr const char *kDirectionalHooksMarker = "/*{{DIRECTIONAL_HOOKS}}*/";
constexpr const char *kEnvironmentHooksMarker = "/*{{ENVIRONMENT_HOOKS}}*/";
constexpr const char *kCompositeHooksMarker = "/*{{COMPOSITE_HOOKS}}*/";
constexpr const char *kAlphaHooksMarker = "/*{{ALPHA_HOOKS}}*/";
// Object2D側のマーカー（struct Material内・main()内のObject2Dブロックに配置）
constexpr const char *kAdditionalMaterialFieldsMarker2D = "/*{{ADDITIONAL_MATERIAL_FIELDS_2D}}*/";
constexpr const char *kModuleLogicMarker2D = "/*{{MODULE_LOGIC_2D}}*/";
constexpr const char *kComposite2DHooksMarker = "/*{{COMPOSITE_HOOKS_2D}}*/";
constexpr const char *kAlpha2DHooksMarker = "/*{{ALPHA_HOOKS_2D}}*/";

// beginマーカーからendマーカーまで（両端含む）を置換する。見つからなければ何もしない
void ReplaceRegion(std::string &source, const std::string &beginMarker, const std::string &endMarker, const std::string &replacement) {
    LogScope scope;
    size_t begin = source.find(beginMarker);
    if (begin == std::string::npos) return;
    size_t end = source.find(endMarker, begin);
    if (end == std::string::npos) return;
    end += endMarker.size();
    source.replace(begin, end - begin, replacement);
}

// マーカー文字列そのものを置換する。見つからなければ何もしない
void ReplaceMarker(std::string &source, const std::string &marker, const std::string &replacement) {
    LogScope scope;
    size_t pos = source.find(marker);
    if (pos == std::string::npos) return;
    source.replace(pos, marker.size(), replacement);
}

const ShaderModuleDefinition *FindModule(const std::vector<ShaderModuleDefinition> &registry, const std::string &token) {
    LogScope scope;
    for (const auto &m : registry) {
        if (m.token == token) return &m;
    }
    return nullptr;
}

} // namespace

bool IsTwoDimensionalSlot(ModuleHookSlot slot) {
    LogScope scope;
    return slot == ModuleHookSlot::Composite2D || slot == ModuleHookSlot::Alpha2D;
}

const std::vector<ShaderModuleDefinition> &GetShaderModuleRegistry() {
    LogScope scope;
    static const std::vector<ShaderModuleDefinition> kModules = {
        { "MultiTone",    ModuleHookSlot::Tone,        0,   "return ApplyMultiTone(halfLambert, mat);" },
        { "RimShade",     ModuleHookSlot::RimColor,    0,   "float3 rimColor = ApplyRimShade(mat, lam);" },
        { "NormalMap",    ModuleHookSlot::PreLighting, 10,  "shadingNormal = ApplyNormalMap(shadingNormal, mat, input.tangent, transformedUV.xy);" },
        { "SpecularMap",  ModuleHookSlot::PreLighting, 20,  "ApplySpecularMap(mat, transformedUV.xy);" },
        { "Backlight",    ModuleHookSlot::Directional, 10,  "ApplyBacklight(lightingColor, mat, shadingNormal, viewDir, toLightDir, light, shadow);" },
        { "EnvironmentMap", ModuleHookSlot::Environment, 10, "envColor = ApplyEnvironmentMap(mat, shadingNormal, input.worldPosition);" },
        { "DetailMap",    ModuleHookSlot::Composite,   2,   "ApplyDetailMap(output.color, mat, transformedUV.xy);" },
        { "AO",           ModuleHookSlot::Composite,   5,   "ApplyAO(output.color, mat, transformedUV.xy);" },
        { "Matcap",       ModuleHookSlot::Composite,   10,  "ApplyMatcap(output.color, mat, shadingNormal);" },
        { "Gradation",    ModuleHookSlot::Composite,   20,  "ApplyGradation(output.color, mat, toonFactor);" },
        { "Emission",     ModuleHookSlot::Composite,   30,  "ApplyEmission(output.color, mat, transformedUV.xy);" },
        { "ColorGrading", ModuleHookSlot::Composite,   100, "ApplyColorGrading(output.color, mat);" },
        { "OpacityMap",   ModuleHookSlot::Alpha,       5,   "ApplyOpacityMap(output.color, mat, transformedUV.xy);" },
        { "DistanceFade", ModuleHookSlot::Alpha,       10,  "ApplyDistanceFade(output.color, mat, input.worldPosition);" },
        { "Dissolve",     ModuleHookSlot::Alpha,       20,  "ApplyDissolve(output.color, mat, input.worldPosition);" },
        { "Vignette2D",        ModuleHookSlot::Composite2D, 10,  "ApplyVignette2D(output.color, mat, transformedUV.xy);" },
        { "GradientOverlay2D", ModuleHookSlot::Composite2D, 5,   "ApplyGradientOverlay2D(output.color, mat, transformedUV.xy);" },
        { "Pulse2D",           ModuleHookSlot::Composite2D, 50,  "ApplyPulse2D(output.color, mat);" },
        { "ColorGrading2D",    ModuleHookSlot::Composite2D, 100, "ApplyColorGrading2D(output.color, mat);" },
        { "FlashColor2D",      ModuleHookSlot::Composite2D, 110, "ApplyFlashColor2D(output.color, mat);" },
        { "MaskTexture2D",     ModuleHookSlot::Alpha2D,     5,   "ApplyMaskTexture2D(output.color, mat, transformedUV.xy);" },
        { "Dissolve2D",        ModuleHookSlot::Alpha2D,     10,  "ApplyDissolve2D(output.color, mat, transformedUV.xy);" },
    };
    return kModules;
}

std::string ComposeAndWriteShader(const std::vector<std::string> &selectedTokens, const std::string &shaderBaseDir) {
    LogScope scope;
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

    // Object3D系フィールド/ロジック（struct Material・ADDITIONAL_MATERIAL_FIELDS/MODULE_LOGICマーカー用）
    std::string additionalFields;
    std::string moduleLogic;
    // Object2D系フィールド/ロジック（Object2DブロックのADDITIONAL_MATERIAL_FIELDS_2D/MODULE_LOGIC_2Dマーカー用）
    std::string additionalFields2D;
    std::string moduleLogic2D;
    std::vector<const ShaderModuleDefinition *> preLightingHooks;
    std::vector<const ShaderModuleDefinition *> directionalHooks;
    std::vector<const ShaderModuleDefinition *> environmentHooks;
    std::vector<const ShaderModuleDefinition *> compositeHooks;
    std::vector<const ShaderModuleDefinition *> alphaHooks;
    std::vector<const ShaderModuleDefinition *> composite2DHooks;
    std::vector<const ShaderModuleDefinition *> alpha2DHooks;
    const ShaderModuleDefinition *toneModule = nullptr;
    const ShaderModuleDefinition *rimColorModule = nullptr;
    std::string combinedName;

    for (const auto &token : sortedTokens) {
        const auto *def = FindModule(registry, token);
        if (!def) continue; // 未登録トークンは無視する

        const bool is2D = IsTwoDimensionalSlot(def->slot);
        if (is2D) {
            additionalFields2D += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Fields.hlsli") + "\n";
            moduleLogic2D += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Logic.hlsli") + "\n";
        } else {
            additionalFields += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Fields.hlsli") + "\n";
            moduleLogic += ReadFileText(shaderBaseDir + "/Modules/" + token + "/Logic.hlsli") + "\n";
        }
        if (!combinedName.empty()) combinedName += ".";
        combinedName += token;

        switch (def->slot) {
        case ModuleHookSlot::Tone: toneModule = def; break;
        case ModuleHookSlot::RimColor: rimColorModule = def; break;
        case ModuleHookSlot::PreLighting: preLightingHooks.push_back(def); break;
        case ModuleHookSlot::Directional: directionalHooks.push_back(def); break;
        case ModuleHookSlot::Environment: environmentHooks.push_back(def); break;
        case ModuleHookSlot::Composite: compositeHooks.push_back(def); break;
        case ModuleHookSlot::Alpha: alphaHooks.push_back(def); break;
        case ModuleHookSlot::Composite2D: composite2DHooks.push_back(def); break;
        case ModuleHookSlot::Alpha2D: alpha2DHooks.push_back(def); break;
        }
    }

    if (combinedName.empty()) return {}; // 有効なモジュールが1つも無かった

    auto byPriority = [](const ShaderModuleDefinition *a, const ShaderModuleDefinition *b) { return a->priority < b->priority; };
    std::sort(preLightingHooks.begin(), preLightingHooks.end(), byPriority);
    std::sort(directionalHooks.begin(), directionalHooks.end(), byPriority);
    std::sort(environmentHooks.begin(), environmentHooks.end(), byPriority);
    std::sort(compositeHooks.begin(), compositeHooks.end(), byPriority);
    std::sort(alphaHooks.begin(), alphaHooks.end(), byPriority);
    std::sort(composite2DHooks.begin(), composite2DHooks.end(), byPriority);
    std::sort(alpha2DHooks.begin(), alpha2DHooks.end(), byPriority);

    auto joinCalls = [](const std::vector<const ShaderModuleDefinition *> &hooks) {
        std::string result;
        for (const auto *h : hooks) result += h->callExpression + "\n";
        return result;
    };

    ReplaceMarker(source, kAdditionalMaterialFieldsMarker, additionalFields);
    ReplaceMarker(source, kModuleLogicMarker, moduleLogic);
    if (toneModule) ReplaceRegion(source, kToneHookBegin, kToneHookEnd, toneModule->callExpression);
    if (rimColorModule) ReplaceRegion(source, kRimColorHookBegin, kRimColorHookEnd, rimColorModule->callExpression);
    ReplaceMarker(source, kPreLightingHooksMarker, joinCalls(preLightingHooks));
    ReplaceMarker(source, kDirectionalHooksMarker, joinCalls(directionalHooks));
    ReplaceMarker(source, kEnvironmentHooksMarker, joinCalls(environmentHooks));
    ReplaceMarker(source, kCompositeHooksMarker, joinCalls(compositeHooks));
    ReplaceMarker(source, kAlphaHooksMarker, joinCalls(alphaHooks));
    ReplaceMarker(source, kAdditionalMaterialFieldsMarker2D, additionalFields2D);
    ReplaceMarker(source, kModuleLogicMarker2D, moduleLogic2D);
    ReplaceMarker(source, kComposite2DHooksMarker, joinCalls(composite2DHooks));
    ReplaceMarker(source, kAlpha2DHooksMarker, joinCalls(alpha2DHooks));

    // Object/ObjectPS.hlslと同じディレクトリへ書き出す。DXCの既定のインクルードハンドラは#pragma onceを
    // インクルードパスの文字列一致で判定しており、ディレクトリを分けると相対インクルード（"../Common/..."等）が
    // 経路によって異なる文字列に展開され、同じファイル（例: Camera3D.hlsli）が二重定義されるエラーになる
    // （実際に発生させて確認済み）。同じディレクトリに置けば、コピー元と全く同じ文字列で解決されるため安全
    const std::string outputPath = shaderBaseDir + "/Object/Generated.Object.Compose." + combinedName + ".hlsl";
    if (!WriteFileText(outputPath, source)) {
        Log("[ShaderModuleComposer] failed to write generated shader: " + outputPath, LogSeverity::Error);
        return {};
    }
    return outputPath;
}

std::vector<std::string> GetModuleFieldNames(const std::string &token, const std::string &shaderBaseDir) {
    LogScope scope;
    const std::string source = ReadFileText(shaderBaseDir + "/Modules/" + token + "/Fields.hlsli");
    static const std::regex kFieldLineRe(R"(^\s*\w+\s+(\w+)\s*(?:\[\s*\d+\s*\])?\s*;)");
    std::vector<std::string> names;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, kFieldLineRe)) {
            names.push_back(match[1].str());
        }
    }
    return names;
}

} // namespace KashipanEngine
