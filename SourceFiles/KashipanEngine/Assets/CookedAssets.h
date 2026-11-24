#pragma once
#include "Utilities/FileIO/ContentCache.h"
#include <stdexcept>

namespace KashipanEngine::CookedAssets {
// ワーカーを起動する前に設定する。ゲームの実行中はマニフェストを変更しない。
inline std::filesystem::path root;
inline ContentCache::Json manifest;
inline bool writing = false;
inline bool Active() { return !writing && manifest.is_object(); }
inline std::string Relative(const std::filesystem::path &path) {
    const auto physical = path.is_absolute() ? path : root / path;
    const auto relative = physical.lexically_normal().lexically_relative(root / "Assets");
    const auto text = ContentCache::PathText(relative);
    if (text.empty() || relative.is_absolute() || text == ".." || text.starts_with("../"))
        throw std::runtime_error("Cooked asset must be inside Assets: " + ContentCache::PathText(path));
    return text;
}
inline void Configure(const std::filesystem::path &projectRoot) {
    root = std::filesystem::absolute(projectRoot).lexically_normal();
    writing = false;
    manifest = nullptr;
    const auto manifestPath = root / "Cooked" / "Manifest.json";
    if (!std::filesystem::exists(manifestPath)) return;
    std::ifstream stream(manifestPath);
    manifest = ContentCache::Json::parse(stream);
    if (manifest.at("version") != 1 || !manifest.at("assets").is_array() || !manifest.at("entries").is_object())
        throw std::runtime_error("Unsupported cooked asset manifest");
    for (const auto &asset : manifest.at("assets")) {
        const auto path = root / "Assets" / ContentCache::Path(asset.get<std::string>());
        const auto relative = Relative(path);
        const auto ext = ContentCache::PathText(path.extension());
        const bool texture = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
            ext == ".dds" || ext == ".hdr" || ext == ".tif" || ext == ".tiff" || ext == ".webp";
        const bool model = ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".dae" ||
            ext == ".3ds" || ext == ".blend" || ext == ".ply" || ext == ".stl" || ext == ".x";
        if ((texture || model) && !manifest.at("entries").contains(std::string(texture ? "Textures|" : "Models|") + relative + "|"))
            throw std::runtime_error("Missing cooked asset manifest entry: " + relative);
    }
    // 各マネージャーがアセットを通知なしに読み飛ばすことを防ぐため、破損や欠落のあるパッケージは事前に拒否する。
    for (const auto &[key, entry] : manifest.at("entries").items()) {
        const auto file = ContentCache::Digest(key) + ".cache";
        if (!std::filesystem::is_regular_file(root / "Cooked" / file))
            throw std::runtime_error("Missing cooked asset: " + key);
    }
}
inline void Begin(const std::filesystem::path &projectRoot) {
    root = std::filesystem::absolute(projectRoot).lexically_normal();
    writing = true;
    manifest = {{"version", 1}, {"assets", ContentCache::Json::array()}, {"entries", ContentCache::Json::object()},
        {"pipelines", ContentCache::Json::object()}};
}
inline std::string Key(const std::string &category, const std::filesystem::path &path, const std::string &variant = {}) {
    return category + "|" + Relative(path) + "|" + variant;
}
inline bool Contains(const std::string &category, const std::filesystem::path &path) {
    if (!Active()) return false;
    return manifest.at("entries").contains(Key(category, path));
}
inline std::optional<std::vector<char>> Load(const std::string &category, const std::filesystem::path &path,
    const std::string &variant = {}) {
    if (!Active()) return std::nullopt;
    const auto key = Key(category, path, variant);
    if (!manifest.at("entries").contains(key))
        throw std::runtime_error("Asset was not cooked: " + key);
    auto bytes = ContentCache::Load(root / "Cooked" / (ContentCache::Digest(key) + ".cache"), key, true);
    if (!bytes) throw std::runtime_error("Invalid cooked asset: " + key);
    return bytes;
}
inline void Store(const std::string &category, const std::filesystem::path &path, const void *data, size_t size,
    const std::string &variant = {}, ContentCache::Json extra = ContentCache::Json::object()) {
    const auto key = Key(category, path, variant);
    if (!writing || !ContentCache::Store(root / "Cooked" / (ContentCache::Digest(key) + ".cache"),
        key, ContentCache::Json::array(), data, size, true)) throw std::runtime_error("Could not cook: " + key);
    manifest["entries"][key] = std::move(extra);
}
// コマンドの処理対象は配布用のステージングディレクトリ内に限る。元のファイルには手を加えない。
int CookDistribution(const std::filesystem::path &projectRoot);
} // namespace KashipanEngine::CookedAssets
