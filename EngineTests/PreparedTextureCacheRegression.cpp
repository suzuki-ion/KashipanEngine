#include "Assets/PreparedTextureCache.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace KashipanEngine;
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    const auto root = std::filesystem::absolute("Generated/PreparedTextureRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto source = root / L"透過画像.tga";
    const auto directory = root / "Cache";
    DirectX::ScratchImage raw;
    assert(SUCCEEDED(raw.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 16, 16, 1, 1)));
    for (size_t i = 0; i < raw.GetPixelsSize(); ++i) raw.GetPixels()[i] = static_cast<uint8_t>(i % 251);
    assert(SUCCEEDED(DirectX::SaveToTGAFile(*raw.GetImages(), source.c_str())));
    auto expected = PreparedTextureCache::PrepareFile(source);
    assert(expected.GetMetadata().mipLevels == 5);
    assert(PreparedTextureCache::Store(source, directory, expected, ContentCache::Dependency(source)));
    DirectX::ScratchImage loaded;
    assert(PreparedTextureCache::Load(source, directory, loaded));
    assert(loaded.GetImageCount() == expected.GetImageCount());
    assert(loaded.GetPixelsSize() == expected.GetPixelsSize());
    assert(std::memcmp(loaded.GetPixels(), expected.GetPixels(), expected.GetPixelsSize()) == 0);
    assert(!PreparedTextureCache::Eligible(root / "animated.gif"));
    assert(!PreparedTextureCache::Eligible(root / "light.hdr"));
    assert(!PreparedTextureCache::Eligible(root / "cube.dds"));
    const auto timestamp = std::filesystem::last_write_time(source);
    raw.GetPixels()[0] ^= 255;
    assert(SUCCEEDED(DirectX::SaveToTGAFile(*raw.GetImages(), source.c_str())));
    std::filesystem::last_write_time(source, timestamp); // Content changes with identical mtime must invalidate.
    assert(!PreparedTextureCache::Load(source, directory, loaded));
    expected = PreparedTextureCache::PrepareFile(source);
    assert(PreparedTextureCache::Store(source, directory, expected, ContentCache::Dependency(source)));
    std::ofstream(PreparedTextureCache::CachePath(directory, source), std::ios::binary | std::ios::trunc) << "broken";
    assert(!PreparedTextureCache::Load(source, directory, loaded));
    std::ofstream(root / "blocked", std::ios::binary) << "file";
    assert(!PreparedTextureCache::Store(source, root / "blocked" / "child", expected, ContentCache::Dependency(source)));
    assert(expected.GetImageCount()); // Cache-write failure does not destroy the usable image.
    std::filesystem::remove_all(root); // Newly created, process-specific directory under Generated.
    CoUninitialize();
    std::cout << "Prepared texture cache regression passed\n";
}
