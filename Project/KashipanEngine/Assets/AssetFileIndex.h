#pragma once
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Conversion/ConvertString.h"
#include <array>
#include <unordered_set>
#include <utility>

namespace KashipanEngine {
/// One startup snapshot. Managers consume routed lists without enumerating Assets again.
class AssetFileIndex final {
public:
    enum class Kind { Texture, Font, BitmapFont, Model, Skeleton, Audio, Animation, Material, Video, Gif, Count };
    explicit AssetFileIndex(std::string root) : root_(std::move(root)) {
        Visit(GetDirectoryData(root_, true, true));
    }
    const std::string &Root() const { return root_; }
    const std::vector<std::string> &Files(Kind kind) const { return files_[static_cast<size_t>(kind)]; }
    size_t FileCount() const { return seen_.size(); }
    size_t DirectoryCount() const { return directories_; }
    /// Called after a startup importer creates a file, before later managers consume the snapshot.
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
        const auto extension = PathToUtf8String(path.extension());
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".dds" || extension == ".hdr" || extension == ".tif" || extension == ".tiff" || extension == ".gif" || extension == ".webp") files_[static_cast<size_t>(Kind::Texture)].push_back(file);
        if (extension == ".ttf" || extension == ".otf") files_[static_cast<size_t>(Kind::Font)].push_back(file);
        if (extension == ".fnt") files_[static_cast<size_t>(Kind::BitmapFont)].push_back(file);
        if (extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds" || extension == ".blend" || extension == ".ply" || extension == ".stl" || extension == ".x") files_[static_cast<size_t>(Kind::Model)].push_back(file);
        if (extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".x" || extension == ".blend" || extension == ".obj") files_[static_cast<size_t>(Kind::Skeleton)].push_back(file);
        if (extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac" || extension == ".aac" || extension == ".m4a" || extension == ".wma") files_[static_cast<size_t>(Kind::Audio)].push_back(file);
        if (extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".x" || extension == ".blend" || extension == ".obj") files_[static_cast<size_t>(Kind::Animation)].push_back(file);
        if (extension == ".mat") files_[static_cast<size_t>(Kind::Material)].push_back(file);
        if (extension == ".mp4" || extension == ".wmv" || extension == ".mov" || extension == ".avi") files_[static_cast<size_t>(Kind::Video)].push_back(file);
        if (extension == ".gif") files_[static_cast<size_t>(Kind::Gif)].push_back(file);
    }
    std::string root_;
    std::array<std::vector<std::string>, static_cast<size_t>(Kind::Count)> files_;
    std::unordered_set<std::string> seen_;
    size_t directories_ = 0;
};
} // namespace KashipanEngine
