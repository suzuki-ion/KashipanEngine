#include "Assets/AssetFileIndex.h"
#include <Windows.h>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
using namespace KashipanEngine;
int main() {
    const auto root = std::filesystem::absolute("Generated/AssetIndexRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root / L"日本語" / "nested");
    const std::vector<std::vector<std::string>> extensions = {
        {".png",".jpg",".jpeg",".bmp",".tga",".dds",".hdr",".tif",".tiff",".gif",".webp"}, {".ttf",".otf"}, {".fnt"},
        {".fbx",".obj",".gltf",".glb",".dae",".3ds",".blend",".ply",".stl",".x"},
        {".fbx",".gltf",".glb",".dae",".x",".blend",".obj"}, {".wav",".mp3",".ogg",".flac",".aac",".m4a",".wma"},
        {".fbx",".gltf",".glb",".dae",".x",".blend",".obj"}, {".mat"}, {".mp4",".wmv",".mov",".avi"}, {".gif"}};
    for (const auto &group : extensions) for (const auto &extension : group) {
        std::ofstream(root / ("root" + extension));
        std::ofstream(root / L"日本語" / ("child" + extension));
    }
    std::ofstream(root / "ignored.PNG"); std::ofstream(root / L"日本語" / "nested" / "scene.json");
    const auto verify = [&](const std::filesystem::path &directory) {
        AssetFileIndex index(PathToUtf8String(directory));
        const auto oldTree = GetDirectoryData(PathToUtf8String(directory), true, true);
        for (size_t kind = 0; kind < extensions.size(); ++kind) {
            std::vector<std::string> expected;
            const std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &data) {
                expected.insert(expected.end(), data.files.begin(), data.files.end());
                for (const auto &child : data.subdirectories) flatten(child);
            };
            flatten(GetDirectoryDataByExtension(oldTree, extensions[kind]));
            assert(expected == index.Files(static_cast<AssetFileIndex::Kind>(kind)));
        }
        return index;
    };
    auto index = verify(root);
    const auto generated = PathToUtf8String(root / L"日本語" / "new.mat");
    std::ofstream(Utf8StringToPath(generated)) << "{}";
    const auto before = index.Files(AssetFileIndex::Kind::Material).size();
    index.AddGeneratedFile(generated); index.AddGeneratedFile(generated);
    assert(index.Files(AssetFileIndex::Kind::Material).size() == before + 1);
    assert(index.Files(AssetFileIndex::Kind::Material).back() == generated);
    // Reading routed lists is a snapshot: a later file is invisible until explicitly registered.
    std::ofstream(root / "late.wav");
    assert(index.Files(AssetFileIndex::Kind::Audio).size() == 14);
    verify("Project/Projects/Test/Assets");
    verify("Project/Projects/JobHuntingGame/Assets");
    AssetFileIndex missing(PathToUtf8String(root / "missing"));
    assert(missing.FileCount() == 0);
    std::filesystem::remove_all(root);
    std::cout << "Asset index regression passed (all 10 routes, order, Unicode, generated material, real projects)\n";
}
