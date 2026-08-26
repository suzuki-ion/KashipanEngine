#include "PipelineVariantResolver.h"

#include <unordered_set>
#include <vector>

#include "Graphics/Pipeline/System/ShaderModuleComposer.h"

namespace KashipanEngine {
namespace {

const std::unordered_set<std::string> &BlendSuffixes() {
    static const std::unordered_set<std::string> kSuffixes = {
        "Add", "Exclusion", "Multiply", "None", "Normal", "Screen", "Subtract", "Translucent",
    };
    return kSuffixes;
}

std::vector<std::string> SplitByDot(const std::string &name) {
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start <= name.size()) {
        size_t dot = name.find('.', start);
        if (dot == std::string::npos) {
            tokens.push_back(name.substr(start));
            break;
        }
        tokens.push_back(name.substr(start, dot - start));
        start = dot + 1;
    }
    return tokens;
}

} // namespace

PipelineVariantResolution TryResolvePipelineVariant(const std::string &pipelineName, const std::string &shaderBaseDir) {
    PipelineVariantResolution result;

    const std::vector<std::string> tokens = SplitByDot(pipelineName);
    if (tokens.size() < 3) return result; // Base + Raster + Blend が最低限必要

    const std::string &base = tokens[0];
    if (base != "Object3D" && base != "Object2D") return result;

    const auto &moduleRegistry = GetShaderModuleRegistry();
    auto findModule = [&](const std::string &token) -> const ShaderModuleDefinition * {
        for (const auto &m : moduleRegistry) {
            if (m.token == token) return &m;
        }
        return nullptr;
    };

    bool isToon = false;
    bool isWorld = false;
    std::string rasterPreset;
    std::string blendPreset;
    std::vector<std::string> moduleTokens;
    std::unordered_set<std::string> seenModuleTokens;

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string &token = tokens[i];
        if (token == "Toon") {
            if (base != "Object3D") return result; // Object2D向けのToonプリセットは存在しない
            isToon = true;
        } else if (token == "World") {
            // エディターのシーンビューが2Dオブジェクトを3D空間内に配置・選択・ギズモ編集できるようにする
            // ための特殊バリアント（gCamera2Dの代わりにgCamera3Dで投影する）。Object3D側は元々
            // gCamera3Dを使っているため意味を持たず、Object2D限定のトークンとする
            if (base != "Object2D") return result;
            isWorld = true;
        } else if (token == "Solid" || token == "DoubleSidedCulling") {
            if (!rasterPreset.empty()) return result; // 重複指定
            rasterPreset = token;
        } else if (token.rfind("Blend", 0) == 0 && token.size() > 5) {
            const std::string suffix = token.substr(5);
            if (!BlendSuffixes().count(suffix)) return result;
            if (!blendPreset.empty()) return result; // 重複指定
            blendPreset = suffix;
        } else if (const auto *def = findModule(token); def && (IsTwoDimensionalSlot(def->slot) == (base == "Object2D"))) {
            // 3D系モジュールはObject3D、2D系（Composite2D/Alpha2D）モジュールはObject2Dのパイプライン名でのみ受理する
            if (!seenModuleTokens.insert(token).second) return result; // 重複指定
            moduleTokens.push_back(token);
        } else {
            return result; // 未知トークン
        }
    }
    if (rasterPreset.empty() || blendPreset.empty()) return result;

    // Translucent（本格的なアルファブレンドによる半透明。Material3D.hlsliのuseAlphaBlend用）は、
    // Object3D.Ghostパイプラインと同じく深度テストのみ・深度書き込み無し（DepthEnableToMaskZero）にする。
    // 重なる半透明オブジェクト同士は自動でソートされないため、RenderPriorityで手動の描画順制御が必要。
    // Object2D+World（エディター限定の3D空間配置バリアント）は、既定パイプライン（BlendNormal）を含め
    // 実質全てのSpriteRendererが対象になるため、Object3Dの通常オブジェクトと同じく常に深度テスト・
    // 書き込み両方を有効にする（DepthEnableToMaskZero＝書き込み無しにすると、3Dオブジェクトが後から
    // 描画された際にスプライトへ深度テストされず、スプライトの手前にあるべき部分が突き抜けて見える）
    const std::string depthStencilPreset = isWorld ? "DepthEnable"
        : (base != "Object3D") ? "DepthDisable"
        : (blendPreset == "Translucent") ? "DepthEnableToMaskZero" : "DepthEnable";
    // TranslucentのBlend方程式はNormalと同一（SrcAlpha/InvSrcAlpha）のため、同じBlendStateプリセットを再利用する
    const std::string blendStatePreset = (blendPreset == "Translucent") ? "Normal" : blendPreset;
    // 既存の静的Pipelines/*.jsonと同じ規則: BlendStateがNormal/None（不透明・非合成）は先に描き、
    // それ以外（Add等の透明合成系、Translucent）は通常描画より後段（RenderPriority=2）で描く
    const int renderPriority = (blendPreset == "Normal" || blendPreset == "None") ? 1 : 2;

    // Worldバリアントは専用のシェーダープリセット（Object2DWorld.Vertex/.Pixel、gCamera3Dで投影する）を使う。
    // PS側の内容自体はObject2Dと同一（カメラを参照しない）が、プリセット名はVertexと対にして登録されている
    const std::string presetBase = isWorld ? (base + "World") : base;

    JSON pixelStage;
    if (moduleTokens.empty()) {
        // モジュール未指定: 既存の静的プリセット（Object3D.Pixel/Object3D.Toon.Pixel）をそのまま使う。
        // 挙動は今まで通り完全に不変
        const std::string pixelPreset = isToon ? (presetBase + ".Toon.Pixel") : (presetBase + ".Pixel");
        pixelStage = { { "UsePreset", pixelPreset } };
    } else {
        // モジュール指定あり: ShaderModuleComposerで合成したHLSLファイルをインラインPathで指定する
        const std::string generatedPath = ComposeAndWriteShader(moduleTokens, shaderBaseDir);
        if (generatedPath.empty()) return result;
        JSON macros = JSON::array({ { { "Name", base } } });
        if (isToon) macros.push_back({ { "Name", "ObjectToon" } });
        pixelStage = {
            { "Path", generatedPath },
            { "EntryPoint", "main" },
            { "TargetProfile", "ps_6_0" },
            { "Macros", macros },
        };
    }

    JSON shaderObj = {
        { "AutoTopologyFromShaders", true },
        { "AutoInputLayoutFromVS", true },
        { "AutoRTCountFromPS", true },
        { "AutoRootDescriptorFromShader", true },
        { "Vertex", { { "UsePreset", presetBase + ".Vertex" } } },
        { "Pixel", pixelStage },
    };
    if (!moduleTokens.empty()) {
        // Pixelステージの登録名を "<pipelineName>.Pixel" にするため、BaseNameへパイプライン名そのものを
        // 入れる（既存の"Object3D.Pixel"等の登録済みシェーダーと名前が衝突しないように）。
        // モジュール未指定時は既存挙動を一切変えないため、この分岐でのみ設定する
        shaderObj["BaseName"] = pipelineName;
    }

    result.synthesizedPipelineJson = {
        { "PipelineType", "Render" },
        { "Name", pipelineName },
        { "Category", (base == "Object3D") ? "3D" : "2D" },
        { "Shader", shaderObj },
        { "RasterizerState", { { "UsePreset", rasterPreset } } },
        { "BlendState", { { "UsePreset", blendStatePreset } } },
        { "DepthStencilState", { { "UsePreset", depthStencilPreset } } },
        { "PipelineState", { { "UsePreset", "Triangle" } } },
        { "RenderPriority", renderPriority },
    };
    result.matched = true;
    return result;
}

} // namespace KashipanEngine
