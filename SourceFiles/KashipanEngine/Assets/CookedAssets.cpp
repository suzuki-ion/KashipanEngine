#include "CookedAssets.h"
#include "Assets/AssetFileIndex.h"
#include "Assets/PreparedTextureCache.h"
#include "Assets/SharedModelImport.h"
#include "Graphics/Pipeline/JsonParser/Shader.h"
#include "Graphics/Pipeline/System/PipelineVariantResolver.h"
#include <algorithm>
#include <regex>
#include <set>

namespace KashipanEngine::CookedAssets {
namespace {
ContentCache::Json ReadJson(const std::filesystem::path &path) {
    std::ifstream stream(path);
    return ContentCache::Json::parse(stream, nullptr, true, true);
}
std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
}

int CookDistribution(const std::filesystem::path &projectRoot) {
    const auto previousDirectory = std::filesystem::current_path();
    const auto target = std::filesystem::absolute(projectRoot).lexically_normal();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int result = 1;
    try {
        // このマーカーは CreateDistribution が管理する。元プロジェクトを誤ってクックしたり削除したりするのを防ぐ。
        if (!std::filesystem::is_regular_file(target / ".cook-staging") ||
            !std::filesystem::is_directory(target / "Assets") || std::filesystem::exists(target / "Project.json"))
            throw std::runtime_error("Cooking requires a marked distribution staging directory");
        if (FAILED(com)) throw std::runtime_error("Cooker COM initialization failed");
        std::filesystem::current_path(target);
        Begin(target);
        AssetFileIndex index(ContentCache::PathText(target / "Assets"));
        std::vector<std::filesystem::path> originals;
        for (const auto &file : index.AllFiles()) manifest["assets"].push_back(Relative(ContentCache::Path(file)));
        std::set<std::string> variants;
        const std::regex variantPattern(R"(Object[23]D(?:\.[A-Za-z0-9_]+){2,})");
        for (const auto &file : std::filesystem::recursive_directory_iterator(target / "Assets")) {
            if (!file.is_regular_file()) continue;
            const auto ext = Lower(file.path().extension().string());
            if (ext == ".json" || ext == ".jsonc" || ext == ".as" || ext == ".mat" || ext == ".prefab" || ext == ".scene") {
                const auto bytes = ContentCache::ReadFile(file.path());
                if (!bytes) throw std::runtime_error("Unreadable cook input");
                const std::string text(bytes->begin(), bytes->end());
                for (std::sregex_iterator it(text.begin(), text.end(), variantPattern), end; it != end; ++it)
                    variants.insert(it->str());
            }
        }
        for (const auto &file : index.Files(AssetFileIndex::Kind::Texture)) {
            const auto path = ContentCache::Path(file);
            const auto ext = Lower(path.extension().string());
            if (ext == ".gif") continue; // GifManager は引き続きアニメーションストリームを必要とする。
            DirectX::ScratchImage decoded;
            DirectX::TexMetadata metadata{};
            HRESULT hr;
            if (ext == ".dds") hr = DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, decoded);
            else if (ext == ".hdr") hr = DirectX::LoadFromHDRFile(path.c_str(), &metadata, decoded);
            else if (ext == ".tga") hr = DirectX::LoadFromTGAFile(path.c_str(), &metadata, decoded);
            else hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, decoded);
            if (FAILED(hr)) throw std::runtime_error("Texture preparation failed: " + file);
            const auto format = ext == ".dds" ? metadata.format : ext == ".hdr" ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
            auto prepared = PreparedTextureCache::ConvertAndGenerateMips(std::move(decoded), metadata, format);
            DirectX::Blob dds;
            if (!prepared.GetImageCount() || FAILED(DirectX::SaveToDDSMemory(prepared.GetImages(), prepared.GetImageCount(),
                prepared.GetMetadata(), DirectX::DDS_FLAGS_NONE, dds))) throw std::runtime_error("DDS preparation failed: " + file);
            Store("Textures", path, dds.GetBufferPointer(), dds.GetBufferSize());
            originals.push_back(path);
        }
        manifest["modelSchema"] = ModelSceneCache::Schema;
        // エンコードにはスカラー型のネイティブなデータ表現を使う。同じエンジンの実行ファイルで配布物をクックし、読み込む。
        manifest["modelABI"] = {sizeof(ai_real), sizeof(aiVectorKey), sizeof(aiQuatKey)};
        for (const auto &file : index.Files(AssetFileIndex::Kind::Model)) {
            const auto path = ContentCache::Path(file);
            Assimp::Importer importer;
            bool externalDependency = false;
            importer.SetIOHandler(new AssimpUtf8IOSystem([&](const char *dependency, bool exists) {
                const auto dep = ContentCache::Path(dependency);
                // すべての依存先がステージング領域の Assets ツリー内にあることを検証する。
                if (exists) {
                    try { Relative(dep); } catch (...) { externalDependency = true; return; }
                    const auto ext = Lower(dep.extension().string());
                    if (ext == ".bin" || ext == ".mtl") originals.push_back(std::filesystem::absolute(dep).lexically_normal());
                }
            }));
            const auto *scene = importer.ReadFile(file, SharedModelImport::ModelFlags);
            if (externalDependency) throw std::runtime_error("Model dependency is outside staged Assets: " + file);
            if (!scene || !scene->mRootNode) throw std::runtime_error("Model preparation failed: " + file + ": " + importer.GetErrorString());
            const auto bytes = ModelSceneCache::Encode(*scene);
            Store("Models", path, bytes.data(), bytes.size());
            originals.push_back(path);
        }

        auto compiler = ShaderCompiler::CreateOfflineCompiler();
        const auto settingsPath = target / "Assets/KashipanEngine/EngineSettings.json";
        const auto engineSettings = ReadJson(settingsPath);
        const auto pipelineSettingsPath = target / ContentCache::Path(engineSettings.value("rendering", ContentCache::Json::object()).value(
            "pipelineSettingsPath", "Assets/KashipanEngine/PipelineSettings.json"));
        const auto settings = ReadJson(pipelineSettingsPath);
        const auto shaderBase = target / ContentCache::Path(settings.at("PresetFolders").at("Shader").get<std::string>());
        std::set<std::string> compiled;
        const auto compile = [&](const ContentCache::Json &shader, const std::filesystem::path &base) {
            auto parsed = Pipeline::JsonParser::ParseShader(shader, base);
            for (auto &stage : parsed.stages) {
                if (stage.isUsePreset) continue;
                const auto &info = stage.compileInfo;
                const auto variant = ContentCache::Json{{"entry", info.entryPoint}, {"target", info.targetProfile}, {"macros", info.macros}}.dump();
                const auto key = Key("Shaders", ContentCache::Path(info.filePath), variant);
                if (compiled.insert(key).second) compiler->CompileShader(info);
            }
        };
        for (const auto &file : std::filesystem::recursive_directory_iterator(shaderBase)) {
            const auto ext = Lower(file.path().extension().string());
            if (!file.is_regular_file() || (ext != ".json" && ext != ".jsonc") || Lower(file.path().stem().string()) == "example") continue;
            const auto json = ReadJson(file.path());
            if (Lower(json.value("Name", "")) != "example") compile(json, file.path().parent_path());
        }
        std::set<std::string> staticPipelineNames;
        const auto pipelineFolder = target / ContentCache::Path(settings.at("PipelineFolder").get<std::string>());
        for (const auto &file : std::filesystem::recursive_directory_iterator(pipelineFolder)) {
            const auto ext = Lower(file.path().extension().string());
            if (!file.is_regular_file() || (ext != ".json" && ext != ".jsonc") || Lower(file.path().stem().string()) == "example") continue;
            const auto json = ReadJson(file.path());
            if (json.contains("Name")) staticPipelineNames.insert(json.at("Name").get<std::string>());
            if (json.contains("Shader")) compile(json.at("Shader"), {});
        }
        const auto extraSettings = target / "Assets/CookSettings.json";
        if (std::filesystem::exists(extraSettings)) {
            for (const auto &name : ReadJson(extraSettings).at("pipelineVariants")) {
                const auto text = name.get<std::string>();
                if (!staticPipelineNames.contains(text) && !TryResolvePipelineVariant(text, ContentCache::PathText(shaderBase)).matched)
                    throw std::runtime_error("Unknown explicitly requested pipeline variant: " + text);
                variants.insert(text);
            }
        }
        for (const auto &name : variants) {
            auto resolved = TryResolvePipelineVariant(name, ContentCache::PathText(shaderBase));
            if (!resolved.matched) continue; // 固定名やパイプラインを指さない文字列は許可する。
            compile(resolved.synthesizedPipelineJson.at("Shader"), {});
            auto &pixel = resolved.synthesizedPipelineJson["Shader"]["Pixel"];
            if (pixel.contains("Path")) pixel["Path"] = Relative(ContentCache::Path(pixel.at("Path").get<std::string>()));
            manifest["pipelines"][name] = std::move(resolved.synthesizedPipelineJson);
        }
        // すべての変換が成功してから、ステージング領域の変換前ファイルを削除し、マニフェストを公開する。
        for (const auto &file : std::filesystem::recursive_directory_iterator(target / "Assets")) {
            const auto ext = Lower(file.path().extension().string());
            if (file.is_regular_file() && (ext == ".hlsl" || ext == ".hlsli")) originals.push_back(file.path());
        }
        for (const auto &path : originals) {
            Relative(path);
            std::filesystem::remove(path);
        }
        std::ofstream output(target / "Cooked/Manifest.json", std::ios::binary);
        output << manifest.dump(2);
        output.close();
        if (!output) throw std::runtime_error("Could not write cooked manifest");
        std::ofstream(target / "Cooked/Cook.log") << "Cooked entries: " << manifest["entries"].size() << "\n";
        result = 0;
    } catch (const std::exception &error) {
        std::ofstream(target / "Cook.error.txt") << error.what() << "\n";
    }
    manifest = nullptr;
    writing = false;
    std::filesystem::current_path(previousDirectory);
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
} // namespace KashipanEngine::CookedAssets
