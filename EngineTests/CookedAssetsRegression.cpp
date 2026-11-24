#include "Assets/CookedAssets.h"
#include "Assets/AssetFileIndex.h"
#include "Assets/SharedModelImport.h"
#include "Graphics/Pipeline/System/PipelineVariantResolver.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Core/ProjectPaths.h"
#include <DirectXTex.h>
#include <cassert>
#include <iostream>

namespace {
std::string projectRoot;
void Write(const std::filesystem::path &path, const std::string &text) { std::ofstream(path, std::ios::binary) << text; }
template<class F> void Reject(F action) { bool rejected = false; try { action(); } catch (const std::exception &) { rejected = true; } assert(rejected); }
}
namespace KashipanEngine {
void Log(const std::string &, LogSeverity) {}
void LogScope::PushPrefix(const std::source_location &) {}
void LogScope::PopPrefix() {}
const std::string &GetTranslationText(const std::string &key) { return key; }
const std::string &ProjectPaths::ProjectRoot() { return projectRoot; }
}

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &) {
    const auto base = std::filesystem::absolute("Generated/CookedRegression-" + std::to_string(GetCurrentProcessId()));
    const auto original = base / L"生成元";
    const auto moved = base / L"移動先";
    const auto shaderFolder = original / "Assets/Shaders";
    const std::filesystem::path shaderName = L"材質.hlsl";
    std::filesystem::create_directories(shaderFolder);
    std::filesystem::create_directories(original / "Assets/Pipelines");
    std::filesystem::create_directories(original / "Assets/KashipanEngine");
    Write(original / ".cook-staging", "");
    const std::string pipelineName = "Object3D.Solid.BlendNormal";
    Write(original / "Assets/CookSettings.json",
        ContentCache::Json{{"pipelineVariants", ContentCache::Json::array({pipelineName})}}.dump());
    Write(original / "Assets/KashipanEngine/EngineSettings.json", "{}");
    Write(original / "Assets/KashipanEngine/PipelineSettings.json",
        R"({"PresetFolders":{"Shader":"Assets/Shaders"},"PipelineFolder":"Assets/Pipelines"})");
    Write(shaderFolder / shaderName, "struct Material { float4 tint; };\nfloat4 main() : SV_Target { return 1; }\n");
    Write(shaderFolder / "test.json", ContentCache::Json{
        {"Name", "test"}, {"Path", ContentCache::PathText(shaderName)}, {"TargetProfile", "ps_6_0"}}.dump());
    Write(original / "Assets/triangle.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");

    DirectX::ScratchImage image;
    assert(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1)));
    std::memset(image.GetPixels(), 127, image.GetPixelsSize());
    assert(SUCCEEDED(DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::DDS_FLAGS_NONE, (original / "Assets/image.dds").c_str())));

    CookedAssets cook(PasskeyForGameEngineMain{});
    const auto cookResult = cook.CookDistribution({}, original);
    if (cookResult != 0) {
        std::cerr << std::ifstream(original / "Cook.error.txt").rdbuf();
        return cookResult;
    }
    assert(!cook.Active());
    assert(!std::filesystem::exists(shaderFolder / shaderName));
    assert(!std::filesystem::exists(original / "Assets/triangle.obj"));
    assert(!std::filesystem::exists(original / "Assets/image.dds"));

    ShaderCompiler::CompileInfo info{"test", ContentCache::PathText(shaderFolder / shaderName), "main", "ps_6_0", {}};
    const auto variant = ContentCache::Json{{"entry", info.entryPoint}, {"target", info.targetProfile}, {"macros", info.macros}}.dump();
    CookedAssets originalAssets(PasskeyForGameEngineMain{});
    originalAssets.Configure({}, original);
    const auto bytecode = originalAssets.Load("Shaders", shaderFolder / shaderName, variant);
    assert(bytecode);
    const auto shaderHash = ContentCache::Digest(bytecode->data(), bytecode->size());

    std::filesystem::rename(original, moved);
    projectRoot = ContentCache::PathText(moved);
    CookedAssets cookedAssets(PasskeyForGameEngineMain{});
    cookedAssets.Configure({}, moved);
    assert(cookedAssets.Active());
    const auto pipeline = cookedAssets.FindPipeline(pipelineName);
    assert(pipeline);
    const auto resolved = TryResolvePipelineVariant(pipelineName, {}, &cookedAssets);
    assert(resolved.matched && resolved.synthesizedPipelineJson == *pipeline);
    CookedAssets uncooked(PasskeyForGameEngineMain{});
    uncooked.Configure({}, base);
    assert(!uncooked.Active() && cookedAssets.Active());
    assert(!uncooked.Load("Textures", "Assets/image.dds"));
    AssetFileIndex index(ContentCache::PathText(moved / "Assets"), &cookedAssets);
    assert(index.Files(AssetFileIndex::Kind::Texture).size() == 1 && index.Files(AssetFileIndex::Kind::Model).size() == 1);
    const auto restoredModel = SharedModelImport::Load(moved / "Assets/triangle.obj", SharedModelImport::ModelFlags, &cookedAssets);
    assert(restoredModel->diskCacheHit && restoredModel->scene->mMeshes[0]->mNumVertices == 3);
    info.filePath = ContentCache::PathText(moved / "Assets/Shaders" / shaderName);
    auto compiler = ShaderCompiler::CreateOfflineCompiler(&cookedAssets);
    const auto *restoredShader = compiler->CompileShader(info);
    assert(ContentCache::Digest(restoredShader->GetBytecodePtr(), restoredShader->GetBytecodeSize()) == shaderHash);
    assert(restoredShader->GetMaterialLayout().totalByteSize == 16);
    assert(restoredShader->GetMaterialLayout().fields.at(0).name == "tint");
    const auto texture = cookedAssets.Load("Textures", moved / "Assets/image.dds");
    DirectX::ScratchImage restoredImage;
    assert(SUCCEEDED(DirectX::LoadFromDDSMemory(reinterpret_cast<const uint8_t *>(texture->data()), texture->size(), DirectX::DDS_FLAGS_NONE, nullptr, restoredImage)));
    assert(std::memcmp(restoredImage.GetPixels(), image.GetPixels(), image.GetPixelsSize()) == 0);
    info.macros = {{"UNCOOKED", "1"}};
    Reject([&] { compiler->CompileShader(info); });
    Reject([&] { cookedAssets.Load("Models", moved / "Assets/unknown.obj"); });
    Reject([&] { cookedAssets.Load("Models", base / "outside.png"); });
    const std::string key = "Textures|image.dds|";
    Write(moved / "Cooked" / (ContentCache::Digest(key) + ".cache"), "broken");
    Reject([&] { cookedAssets.Load("Textures", moved / "Assets/image.dds"); });
    std::filesystem::remove(moved / "Cooked" / (ContentCache::Digest(key) + ".cache"));
    CookedAssets invalid(PasskeyForGameEngineMain{});
    Reject([&] { invalid.Configure({}, moved); });
    assert(!invalid.Active());
    // このプロセスが新たに作成したテスト用データだけを削除する。
    std::filesystem::remove_all(base);
    std::cout << "Cooked asset relocation, source-free restoration, material layout and failure regression passed\n";
    return 0;
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return KashipanEngine::Execute({}, {});
}

int main() {
    return WinMain(nullptr, nullptr, nullptr, 0);
}
