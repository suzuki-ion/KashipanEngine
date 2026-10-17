#include "Assets/PreparedTextureCache.h"
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) { std::cerr << "Usage: PrepareStartupTextures <project directory>\n"; return 2; }
    const auto root = std::filesystem::absolute(argv[1]);
    const auto assets = root / "Assets";
    if (!std::filesystem::is_directory(assets)) { std::cerr << "Assets directory not found\n"; return 2; }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com)) return 2;
    size_t hits = 0, converted = 0, failures = 0;
    try {
        const auto directory = KashipanEngine::ContentCache::Directory(KashipanEngine::ContentCache::PathText(root), "Textures");
        for (const auto &file : std::filesystem::recursive_directory_iterator(assets)) {
            if (!file.is_regular_file() || !KashipanEngine::PreparedTextureCache::Eligible(file.path())) continue;
            DirectX::ScratchImage image;
            if (KashipanEngine::PreparedTextureCache::Load(file.path(), directory, image)) { ++hits; continue; }
            const auto dependency = KashipanEngine::ContentCache::Dependency(file.path());
            image = KashipanEngine::PreparedTextureCache::PrepareFile(file.path());
            if (KashipanEngine::PreparedTextureCache::Store(file.path(), directory, image, dependency)) ++converted;
            else { ++failures; std::cerr << "Failed: " << KashipanEngine::ContentCache::PathText(file.path()) << '\n'; }
        }
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; ++failures; }
    CoUninitialize();
    std::cout << "Prepared textures: hits=" << hits << ", converted=" << converted << ", failures=" << failures << '\n';
    return failures ? 1 : 0;
}
