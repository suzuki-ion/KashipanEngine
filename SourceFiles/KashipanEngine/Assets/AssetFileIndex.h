#pragma once
#include "Utilities/FileIO/Directory.h"
#include "Assets/AssetPath.h"
#include "Assets/CookedAssets.h"
#include <array>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace KashipanEngine {
/// 起動時のスナップショット。各マネージャーはAssetsを再列挙せず、振り分け済みの一覧を使用する。
class AssetFileIndex final {
public:
    enum class Kind { Texture, Font, BitmapFont, Model, Skeleton, Audio, Animation, Material, Video, Gif, Count };

    static bool SupportsFile(Kind kind, const std::filesystem::path &path) {
        return SupportsExtension(kind, AssetPath::ToLower(path.extension().string()));
    }

private:
    static bool SupportsExtension(Kind kind, std::string_view extension) noexcept {
        switch (kind) {
        case Kind::Texture:
            return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
                extension == ".tga" || extension == ".dds" || extension == ".hdr" || extension == ".tif" ||
                extension == ".tiff" || extension == ".gif" || extension == ".webp";
        case Kind::Font:
            return extension == ".ttf" || extension == ".otf";
        case Kind::BitmapFont:
            return extension == ".fnt";
        case Kind::Model:
            return extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb" ||
                extension == ".dae" || extension == ".3ds" || extension == ".blend" || extension == ".ply" ||
                extension == ".stl" || extension == ".x";
        case Kind::Animation:
        case Kind::Skeleton:
            return extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" ||
                extension == ".x" || extension == ".blend" || extension == ".obj";
        case Kind::Audio:
            return extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac" ||
                extension == ".aac" || extension == ".m4a" || extension == ".wma";
        case Kind::Material:
            return extension == ".mat";
        case Kind::Video:
            return extension == ".mp4" || extension == ".wmv" || extension == ".mov" || extension == ".avi";
        case Kind::Gif:
            return extension == ".gif";
        case Kind::Count:
            break;
        }
        return false;
    }

public:
    explicit AssetFileIndex(std::string root) : root_(std::move(root)) {
        if (CookedAssets::Active()) {
            for (const auto &file : CookedAssets::manifest.at("assets"))
                Add(ContentCache::PathText(ContentCache::Path(root_) / ContentCache::Path(file.get<std::string>())));
        } else Visit(GetDirectoryData(root_, true, true));
    }
    const std::string &Root() const { return root_; }
    const std::vector<std::string> &Files(Kind kind) const { return files_[static_cast<size_t>(kind)]; }
    const std::vector<std::string> &AllFiles() const { return allFiles_; }
    size_t FileCount() const { return seen_.size(); }
    size_t DirectoryCount() const { return directories_; }
    /// 起動時のインポーターがファイルを生成した後、後続のマネージャーがスナップショットを使用する前に呼び出す。
    void AddGeneratedFile(const std::string &file) { Add(file); }
private:
    void Visit(const DirectoryData &directory) {
        ++directories_;
        for (const auto &file : directory.files) Add(file);
        for (const auto &child : directory.subdirectories) Visit(child);
    }
    void Add(const std::string &file) {
        const auto path = Utf8StringToPath(file);
        const auto identity = PathToUtf8String(std::filesystem::absolute(path).lexically_normal());
        if (!seen_.insert(identity).second) return;
        allFiles_.push_back(file);
        const auto extension = PathToUtf8String(path.extension());
        for (size_t i = 0; i < files_.size(); ++i) {
            if (SupportsExtension(static_cast<Kind>(i), extension)) files_[i].push_back(file);
        }
    }
    std::string root_;
    std::vector<std::string> allFiles_;
    std::array<std::vector<std::string>, static_cast<size_t>(Kind::Count)> files_;
    std::unordered_set<std::string> seen_;
    size_t directories_ = 0;
};
} // namespace KashipanEngine
