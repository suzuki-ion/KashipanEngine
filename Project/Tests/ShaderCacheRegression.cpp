#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/ContentCache.h"
#include <cassert>
#include <iostream>
namespace {
std::string projectRoot;
std::vector<std::string> messages;
void Write(const std::filesystem::path &path, const std::string &text) { std::ofstream(path, std::ios::binary) << text; }
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
        ShaderCompiler compiler(Passkey<PipelineManager>{}, nullptr);
        ShaderCompiler::CompileInfo info{"test", ContentCache::PathText(root / L"試験.hlsl"), "main", "ps_6_0", {{"COLOR", "1"}}};
        Write(root / "color.hlsli", "float GetColor() { return COLOR; }\n");
        Write(root / L"試験.hlsl", "#include \"color.hlsli\"\nfloat4 main() : SV_Target { return GetColor(); }\n");
        auto *first = compiler.CompileShader(info);
        assert(first && first->GetBytecodePtr());
        const auto original = ContentCache::Digest(first->GetBytecodePtr(), first->GetBytecodeSize());
        auto *second = compiler.CompileShader(info);
        assert(ContentCache::Digest(second->GetBytecodePtr(), second->GetBytecodeSize()) == original);
        compiler.LogCacheStatistics();
        assert(messages.back().find("hits=1, misses=1") != std::string::npos);
        Write(root / "color.hlsli", "float GetColor() { return COLOR * 0.5; }\n");
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
        assert(rejected); // Changed invalid input cannot silently reuse stale bytecode.
    }
};
}
int main() {
    auto root = std::filesystem::absolute("Generated/ShaderCacheRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    projectRoot = KashipanEngine::ContentCache::PathText(root);
    KashipanEngine::PipelineManager::Run(root);
    // This test owns a newly created, process-specific directory under Generated.
    std::filesystem::remove_all(root);
    std::cout << "Shader cache regression passed\n";
}
