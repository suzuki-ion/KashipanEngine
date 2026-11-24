#pragma once
#include "Utilities/FileIO/ContentCache.h"
#include "Utilities/Passkeys.h"
#include <stdexcept>

namespace KashipanEngine {

class ShaderCompiler;

/// @brief Cookedアセットの管理クラス。KashipanEngine.hのExecute()からのみ生成される。
class CookedAssets final {
public:
    explicit CookedAssets(PasskeyForGameEngineMain) {}
    CookedAssets(const CookedAssets &) = delete;
    CookedAssets &operator=(const CookedAssets &) = delete;
    CookedAssets(CookedAssets &&) = delete;
    CookedAssets &operator=(CookedAssets &&) = delete;

    void Configure(PasskeyForGameEngineMain, const std::filesystem::path &projectRoot);
    int CookDistribution(PasskeyForGameEngineMain, const std::filesystem::path &projectRoot);

    bool Active() const { return !isWriting_ && manifest_.is_object(); }

    bool Contains(const std::string &category, const std::filesystem::path &path) const {
        if (!Active()) return false;
        return manifest_.at("entries").contains(Key(category, path));
    }
    std::optional<std::vector<char>> Load(const std::string &category, const std::filesystem::path &path,
        const std::string &variant = {}) const {
        if (!Active()) return std::nullopt;
        const auto key = Key(category, path, variant);
        if (!manifest_.at("entries").contains(key))
            throw std::runtime_error("Asset was not cooked: " + key);
        auto bytes = ContentCache::Load(root_ / "Cooked" / (ContentCache::Digest(key) + ".cache"), key, true);
        if (!bytes) throw std::runtime_error("Invalid cooked asset: " + key);
        return bytes;
    }

    std::vector<std::string> AssetPaths() const {
        return manifest_.at("assets").get<std::vector<std::string>>();
    }

    bool IsModelFormatCompatible(const std::string &schema, const ContentCache::Json &abi) const {
        return manifest_.at("modelSchema") == schema && manifest_.at("modelABI") == abi;
    }

    const ContentCache::Json &ShaderLayout(const std::filesystem::path &path, const std::string &variant) const {
        return manifest_.at("entries").at(Key("Shaders", path, variant));
    }

    std::optional<ContentCache::Json> FindPipeline(const std::string &name) const {
        if (!Active()) return std::nullopt;
        const auto &pipelines = manifest_.at("pipelines");
        const auto found = pipelines.find(name);
        if (found == pipelines.end()) return std::nullopt;
        auto pipeline = *found;
        auto &pixel = pipeline["Shader"]["Pixel"];
        if (pixel.contains("Path")) {
            pixel["Path"] = ContentCache::PathText(root_ / "Assets" / ContentCache::Path(pixel.at("Path").get<std::string>()));
        }
        return pipeline;
    }

    void StoreShader(Passkey<ShaderCompiler>, const std::filesystem::path &path, const void *data, size_t size,
        const std::string &variant, ContentCache::Json layout) {
        Store("Shaders", path, data, size, variant, std::move(layout));
    }

private:
    std::string Relative(const std::filesystem::path &path) const {
        const auto physical = path.is_absolute() ? path : root_ / path;
        const auto relative = physical.lexically_normal().lexically_relative(root_ / "Assets");
        const auto text = ContentCache::PathText(relative);
        if (text.empty() || relative.is_absolute() || text == ".." || text.starts_with("../"))
            throw std::runtime_error("Cooked asset must be inside Assets: " + ContentCache::PathText(path));
        return text;
    }

    std::string Key(const std::string &category, const std::filesystem::path &path, const std::string &variant = {}) const {
        return category + "|" + Relative(path) + "|" + variant;
    }

    void Store(const std::string &category, const std::filesystem::path &path, const void *data, size_t size,
        const std::string &variant = {}, ContentCache::Json extra = ContentCache::Json::object()) {
        const auto key = Key(category, path, variant);
        if (!isWriting_ || !ContentCache::Store(root_ / "Cooked" / (ContentCache::Digest(key) + ".cache"),
            key, ContentCache::Json::array(), data, size, true)) throw std::runtime_error("Could not cook: " + key);
        manifest_["entries"][key] = std::move(extra);
    }

    std::filesystem::path root_;
    ContentCache::Json manifest_;
    bool isWriting_ = false;
};

} // namespace KashipanEngine
