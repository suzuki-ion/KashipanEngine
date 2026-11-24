#include "Assets/CookedAssets.h"
#include "Assets/AssetFileIndex.h"
#include "Assets/SharedModelImport.h"
#include "Assets/PreparedTextureCache.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Core/ProjectPaths.h"
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
int main() {
    using namespace KashipanEngine;
    const auto base = std::filesystem::absolute("Generated/CookedRegression-" + std::to_string(GetCurrentProcessId()));
    const auto original = base / L"生成元";
    const auto moved = base / L"移動先";
    std::filesystem::create_directories(original / "Assets");
    projectRoot = ContentCache::PathText(original);
    CookedAssets::Begin(original);
    Write(original / L"Assets/材質.hlsl", "struct Material { float4 tint; };\nfloat4 main() : SV_Target { return 1; }\n");
    auto compiler = ShaderCompiler::CreateOfflineCompiler();
    ShaderCompiler::CompileInfo info{"test", ContentCache::PathText(original / L"Assets/材質.hlsl"), "main", "ps_6_0", {}};
    const auto *shader = compiler->CompileShader(info);
    const auto shaderHash = ContentCache::Digest(shader->GetBytecodePtr(), shader->GetBytecodeSize());
    const auto layoutSize = shader->GetMaterialLayout().totalByteSize;
    assert(layoutSize == 16);
    Write(original / "Assets/triangle.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Assimp::Importer importer;
    const auto *scene = importer.ReadFile(ContentCache::PathText(original / "Assets/triangle.obj"), SharedModelImport::ModelFlags);
    assert(scene && scene->mNumMeshes == 1);
    const auto model = ModelSceneCache::Encode(*scene);
    CookedAssets::Store("Models", original / "Assets/triangle.obj", model.data(), model.size());
    CookedAssets::manifest["modelSchema"] = ModelSceneCache::Schema;
    CookedAssets::manifest["modelABI"] = {sizeof(ai_real), sizeof(aiVectorKey), sizeof(aiQuatKey)};
    DirectX::ScratchImage image;
    assert(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1)));
    std::memset(image.GetPixels(), 127, image.GetPixelsSize());
    DirectX::Blob dds;
    assert(SUCCEEDED(DirectX::SaveToDDSMemory(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, dds)));
    CookedAssets::Store("Textures", original / "Assets/image.png", dds.GetBufferPointer(), dds.GetBufferSize());
    CookedAssets::manifest["assets"] = {"image.png", "triangle.obj"};
    Write(original / "Cooked/Manifest.json", CookedAssets::manifest.dump());
    // 入力アセットをすべて削除し、パッケージの移動と作業ディレクトリの変更を行う。
    std::filesystem::remove(original / L"Assets/材質.hlsl");
    std::filesystem::remove(original / "Assets/triangle.obj");
    std::filesystem::rename(original, moved);
    projectRoot = ContentCache::PathText(moved);
    CookedAssets::Configure(moved);
    assert(CookedAssets::Active());
    AssetFileIndex index(ContentCache::PathText(moved / "Assets"));
    assert(index.Files(AssetFileIndex::Kind::Texture).size() == 1 && index.Files(AssetFileIndex::Kind::Model).size() == 1);
    const auto restoredModel = SharedModelImport::Load(moved / "Assets/triangle.obj", SharedModelImport::ModelFlags);
    assert(restoredModel->diskCacheHit && restoredModel->scene->mMeshes[0]->mNumVertices == 3);
    info.filePath = ContentCache::PathText(moved / L"Assets/材質.hlsl");
    const auto *restoredShader = compiler->CompileShader(info);
    assert(ContentCache::Digest(restoredShader->GetBytecodePtr(), restoredShader->GetBytecodeSize()) == shaderHash);
    assert(restoredShader->GetMaterialLayout().totalByteSize == layoutSize);
    assert(restoredShader->GetMaterialLayout().fields.at(0).name == "tint");
    const auto texture = CookedAssets::Load("Textures", moved / "Assets/image.png");
    DirectX::ScratchImage restoredImage;
    assert(SUCCEEDED(DirectX::LoadFromDDSMemory(reinterpret_cast<const uint8_t *>(texture->data()), texture->size(), DirectX::DDS_FLAGS_NONE, nullptr, restoredImage)));
    assert(std::memcmp(restoredImage.GetPixels(), image.GetPixels(), image.GetPixelsSize()) == 0);
    info.macros = {{"UNCOOKED", "1"}};
    Reject([&] { compiler->CompileShader(info); });
    Reject([&] { CookedAssets::Load("Models", moved / "Assets/unknown.obj"); });
    Reject([&] { CookedAssets::Relative(base / "outside.png"); });
    const auto key = CookedAssets::Key("Textures", moved / "Assets/image.png");
    Write(moved / "Cooked" / (ContentCache::Digest(key) + ".cache"), "broken");
    Reject([&] { CookedAssets::Load("Textures", moved / "Assets/image.png"); });
    std::filesystem::remove(moved / "Cooked" / (ContentCache::Digest(key) + ".cache"));
    Reject([&] { CookedAssets::Configure(moved); });
    CookedAssets::manifest = nullptr;
    // このプロセスが新たに作成したテスト用データだけを削除する。
    std::filesystem::remove_all(base);
    std::cout << "Cooked asset relocation, source-free restoration, material layout and failure regression passed\n";
}
