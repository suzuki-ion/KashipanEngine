#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/ContentCache.h"
#include <cassert>
#include <iostream>
#include <algorithm>
namespace {
std::string projectRoot;
std::vector<std::string> messages;
void Write(const std::filesystem::path &path, const std::string &text) { std::ofstream(path, std::ios::binary) << text; }
size_t Count(const std::string &text) {
    return static_cast<size_t>(std::count_if(messages.begin(), messages.end(), [&](const auto &message) { return message.find(text) != std::string::npos; }));
}
void CheckMaterialLayoutCache(const std::filesystem::path &root) {
    using namespace KashipanEngine;
    const auto source = root / "layout.hlsl";
    const auto include = root / "layout.hlsli";
    const auto sourceText = "#include \"layout.hlsli\"\n#ifdef UNUSED\n#include \"later.hlsli\"\n#endif\nTexture2D image : register(t0);\nTextureCube sky : register(t1);\n";
    const std::string fields = "struct Material {\nfloat value; // @Range(0, 1, 0.1)\n#ifdef EXTRA\nfloat4 tint; // @Color\n#endif\n};\n";
    Write(source, sourceText); Write(include, fields);
    const auto path = ContentCache::PathText(source);
    auto first = MaterialLayout::BuildCachedFromHlslSource(path, {});
    assert(first.totalByteSize == 4 && first.fields.size() == 1 && first.fields[0].hasRange);
    assert(first.textureFields == std::vector<std::string>{"image"} && first.textureCubeFields == std::vector<std::string>{"sky"});
    auto hits = Count("[MaterialLayoutCache] hit:");
    auto second = MaterialLayout::BuildCachedFromHlslSource(path, {});
    assert(second.fields[0].rangeStep == first.fields[0].rangeStep && Count("[MaterialLayoutCache] hit:") == hits + 1);
    // サイズと更新日時が同じでも、インクルードやコメント内の注釈の変更でキャッシュを無効化する必要がある。
    const auto stamp = std::filesystem::last_write_time(include);
    auto edited = fields; edited.replace(edited.find("0, 1"), 4, "0, 2");
    Write(include, edited); std::filesystem::last_write_time(include, stamp);
    auto changed = MaterialLayout::BuildCachedFromHlslSource(path, {});
    assert(changed.fields[0].rangeMax == 2 && Count("[MaterialLayoutCache] hit:") == hits + 1);
    auto macro = MaterialLayout::BuildCachedFromHlslSource(path, {"EXTRA"});
    assert(macro.totalByteSize == 20 && macro.fields.size() == 2 && macro.fields[1].valueType == ValueType::Color);
    // レイアウトパーサーは条件分岐を取り除く前にソース上のインクルードを走査するため、それらも追跡する。
    auto parses = Count("[MaterialLayoutCache] parsed:");
    Write(root / "later.hlsli", "// newly present\n");
    MaterialLayout::BuildCachedFromHlslSource(path, {});
    assert(Count("[MaterialLayoutCache] parsed:") == parses + 1);
    std::filesystem::remove(include);
    assert(MaterialLayout::BuildCachedFromHlslSource(path, {}).fields.empty());
    Write(include, fields);
    assert(MaterialLayout::BuildCachedFromHlslSource(path, {}).totalByteSize == 4);
    for (const auto &file : std::filesystem::directory_iterator(root / "Cache/Startup/MaterialLayouts")) Write(file.path(), "corrupt");
    assert(MaterialLayout::BuildCachedFromHlslSource(path, {}).totalByteSize == 4);
    hits = Count("[MaterialLayoutCache] hit:");
    MaterialLayout::BuildCachedFromHlslSource(path, {});
    assert(Count("[MaterialLayoutCache] hit:") == hits + 1);
    // コンテナのチェックサムが正しくても、レイアウトのスキーマが不正なら修復する。
    for (const auto &file : std::filesystem::directory_iterator(root / "Cache/Startup/MaterialLayouts")) {
        auto bytes = ContentCache::ReadFile(file.path());
        if (!bytes || bytes->size() <= sizeof(uint64_t)) continue;
        uint64_t length = 0; std::memcpy(&length, bytes->data(), sizeof(length));
        if (length >= bytes->size() - sizeof(length)) continue;
        auto metadata = ContentCache::Json::parse(bytes->begin() + sizeof(length), bytes->begin() + sizeof(length) + length);
        auto identity = ContentCache::Json::parse(metadata.at("key").get<std::string>());
        if (identity.at("path") != ContentCache::PathText(std::filesystem::absolute(source).lexically_normal()) || !identity.at("macros").empty()) continue;
        const std::string invalid = "{}";
        assert(ContentCache::Store(file.path(), metadata.at("key"), metadata.at("dependencies"), invalid.data(), invalid.size()));
        assert(MaterialLayout::BuildCachedFromHlslSource(path, {}).totalByteSize == 4);
        hits = Count("[MaterialLayoutCache] hit:");
        MaterialLayout::BuildCachedFromHlslSource(path, {});
        assert(Count("[MaterialLayoutCache] hit:") == hits + 1);
    }
    // 新しいソースには以前のキャッシュがない。キャッシュの保存先を使えなくても解析は成功する必要がある。
    const auto blockedSource = root / "blocked.hlsl"; Write(blockedSource, fields);
    auto directory = root / "Cache/Startup/MaterialLayouts";
    auto savedDirectory = root / "SavedMaterialLayouts";
    std::filesystem::rename(directory, savedDirectory); Write(directory, "not a directory");
    assert(MaterialLayout::BuildCachedFromHlslSource(ContentCache::PathText(blockedSource), {}).totalByteSize == 4);
    std::filesystem::remove(directory); std::filesystem::rename(savedDirectory, directory);
}
}
namespace KashipanEngine {
void Log(const std::string &text, LogSeverity) { messages.push_back(text); }
void LogScope::PushPrefix(const std::source_location &) {}
void LogScope::PopPrefix() {}
const std::string &GetTranslationText(const std::string &key) { return key; }
const std::string &ProjectPaths::ProjectRoot() { return projectRoot; }
class PipelineManager {
public:
    static void Run(const std::filesystem::path &root) {
        auto offlineCompiler = ShaderCompiler::CreateOfflineCompiler();
        auto &compiler = *offlineCompiler;
        ShaderCompiler::CompileInfo info{"test", ContentCache::PathText(root / L"試験.hlsl"), "main", "ps_6_0", {{"COLOR", "1"}}};
        Write(root / "color.hlsli", "float GetColor() { return COLOR; }\n");
        Write(root / L"試験.hlsl", "#include \"color.hlsli\"\nfloat4 main() : SV_Target { return GetColor(); }\n");
        auto *first = compiler.CompileShader(info);
        assert(first && first->GetBytecodePtr());
        const auto original = ContentCache::Digest(first->GetBytecodePtr(), first->GetBytecodeSize());
        auto *second = compiler.CompileShader(info);
        assert(first != second && first->GetBytecodePtr() == second->GetBytecodePtr());
        assert(ContentCache::Digest(second->GetBytecodePtr(), second->GetBytecodeSize()) == original);
        compiler.LogCacheStatistics();
        assert(messages.back().find("hits=1, misses=1") != std::string::npos);
        assert(Count("Shader reuse: blobs=1, reflections=1") == 1);
        const auto includeStamp = std::filesystem::last_write_time(root / "color.hlsli");
        Write(root / "color.hlsli", "float GetColor() { return 0.125; }\n");
        std::filesystem::last_write_time(root / "color.hlsli", includeStamp);
        auto *changed = compiler.CompileShader(info);
        assert(ContentCache::Digest(changed->GetBytecodePtr(), changed->GetBytecodeSize()) != original);
        info.macros[0].second = "2";
        compiler.CompileShader(info);
        for (const auto &file : std::filesystem::directory_iterator(root / "Cache/Startup/Shaders")) Write(file.path(), "corrupt");
        compiler.CompileShader(info);
        compiler.LogCacheStatistics();
        assert(messages.back().find("hits=1, misses=4") != std::string::npos);
        Write(root / "color.hlsli", "this is invalid HLSL");
        bool rejected = false;
        try { compiler.CompileShader(info); } catch (const std::exception &) { rejected = true; }
        assert(rejected); // 入力が不正な内容に変わった場合は、古いバイトコードをそのまま再利用してはならない。
        // コンパイル結果の独立したハンドルは、再利用状態のリセットや別のハンドルの破棄後も有効。
        compiler.ClearReuseCache(Passkey<PipelineManager>{});
        compiler.DestroyShader(first);
        assert(ContentCache::Digest(second->GetBytecodePtr(), second->GetBytecodeSize()) == original);
        Write(root / "reflection.hlsl", "cbuffer Params : register(b0) { float4 color; };\nfloat4 main() : SV_Target { return color; }\n");
        info.filePath = ContentCache::PathText(root / "reflection.hlsl");
        auto *reflected = compiler.CompileShader(info);
        auto *shared = compiler.CompileShader(info);
        assert(reflected != shared && reflected->GetBytecodePtr() == shared->GetBytecodePtr());
        assert(shared->GetReflectionInfo().GetResourceBindingInfo("Params") && shared->GetReflectionInfo().ShaderVariables().contains("Params"));
        compiler.ClearReuseCache(Passkey<PipelineManager>{});
        compiler.DestroyShader(reflected);
        assert(shared->GetReflectionInfo().GetResourceBindingInfo("Params"));
        const auto oldIDs = ShaderCompiler::CaptureCompiledShaderIDs(Passkey<PipelineManager>{});
        compiler.ClearReuseCache(Passkey<PipelineManager>{});
        Write(root / "reflection.hlsl", "cbuffer NewParams : register(b1) { float4 changedColor; };\nfloat4 main() : SV_Target { return changedColor; }\n");
        auto *newGeneration = compiler.CompileShader(info);
        assert(newGeneration->GetReflectionInfo().GetResourceBindingInfo("NewParams"));
        // パイプラインの再読み込みをロールバックするときは、新しいハンドルだけを破棄して従来の結果を保持する。
        ShaderCompiler::DestroyCompiledShadersExcept(Passkey<PipelineManager>{}, oldIDs);
        assert(shared->GetReflectionInfo().GetResourceBindingInfo("Params"));
        compiler.ClearReuseCache(Passkey<PipelineManager>{});
        newGeneration = compiler.CompileShader(info);
        ShaderCompiler::DestroyCompiledShaders(Passkey<PipelineManager>{}, oldIDs);
        assert(newGeneration->GetReflectionInfo().GetResourceBindingInfo("NewParams"));
    }
};
}
int main() {
    auto root = std::filesystem::absolute("Generated/ShaderCacheRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    projectRoot = KashipanEngine::ContentCache::PathText(root);
    KashipanEngine::PipelineManager::Run(root);
    CheckMaterialLayoutCache(root);
    // このテストは Generated の下に新規作成したプロセス専用のディレクトリを使用する。
    std::filesystem::remove_all(root);
    std::cout << "Shader cache regression passed\n";
}
