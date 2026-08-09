#include "FontManager.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <wrl.h>

#include "Assets/CaseInsensitive.h"
#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Graphics/IShaderTexture.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/RawFile.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

namespace KashipanEngine {

namespace {

using FontHandle = FontManager::FontHandle;
using GlyphInfo = FontManager::GlyphInfo;

/// @brief SDFの縁からのパディング（テクセル単位。太字・拡大時ににじみが出ないよう余裕を持たせる）
constexpr int kSdfPadding = static_cast<int>(FontManager::kSdfPixelRange);
/// @brief SDFの「輪郭上」を表す値
constexpr unsigned char kSdfOnEdgeValue = 128;
/// @brief 下線・取り消し線描画用の合成グリフ（常にベタ塗り）の一辺サイズ
constexpr std::uint32_t kSolidGlyphSize = 8;
/// @brief アトラスサイズの上限（これ以上は拡張しない）
constexpr std::uint32_t kMaxAtlasSize = 4096;

class AtlasTextureView;

/// @brief フォント1つ分の内部管理データ
struct FontEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;
    std::string name;

    /// @brief フォントファイルの生データ（stbtt_fontinfoが参照し続けるため、フォント破棄まで保持する）
    std::vector<unsigned char> fileData;
    std::unique_ptr<stbtt_fontinfo> info;

    std::unordered_map<char32_t, GlyphInfo> glyphs;
    GlyphInfo solidGlyph;
    bool hasSolidGlyph = false;

    /// @brief アトラスのCPU側ピクセルバッファ（R8、1バイト/ピクセル）
    std::vector<unsigned char> atlasPixels;
    std::uint32_t atlasSize = 512;
    /// @brief シェルフパッキングの現在位置
    std::uint32_t packCursorX = 0;
    std::uint32_t packCursorY = 0;
    std::uint32_t packRowHeight = 0;

    std::unique_ptr<ShaderResourceResource> atlasTexture;
    std::unique_ptr<AtlasTextureView> atlasView;
    TextureManager::TextureHandle atlasTextureHandle = TextureManager::kInvalidHandle;
};

/// @brief FontEntryが持つアトラステクスチャをTextureManagerへ「外部管理テクスチャ」として見せるためのラッパー
/// @details SRVは呼び出しのたびにowner_->atlasTextureから取得するため、グリフ追加でテクスチャが
///          差し替わっても（ScreenBufferのダブルバッファ切り替えと同様に）常に最新のSRVを返せる
class AtlasTextureView final : public IShaderTexture {
public:
    explicit AtlasTextureView(FontEntry *owner) : owner_(owner) {}

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override {
        return owner_->atlasTexture ? owner_->atlasTexture->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
    std::uint32_t GetWidth() const noexcept override { return owner_->atlasSize; }
    std::uint32_t GetHeight() const noexcept override { return owner_->atlasSize; }

private:
    FontEntry *owner_ = nullptr;
};

std::string sAssetsRootPath;
std::unordered_map<FontHandle, FontEntry> sFonts;
FileMap<FontHandle> sFileNameToHandle;
FileMap<FontHandle> sAssetPathToHandle;
FileMap<FontHandle> sNameToHandle;
FontHandle sNextHandle = 1;

ID3D12Device *sDevice = nullptr;
DirectXCommon *sDirectXCommon = nullptr;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedFontExtension(const std::filesystem::path &p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".ttf" || ext == ".otf");
}

std::string MakeAssetRelativePath(const std::string &assetsRoot, const std::string &fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

/// @brief アトラス内にwidth×heightの矩形を確保する（シェルフパッキング）
/// @return 確保できた場合はtrue（outX/outYに左上座標を格納）、満杯の場合はfalse
bool PackRect(FontEntry &font, int width, int height, std::uint32_t &outX, std::uint32_t &outY) {
    const auto w = static_cast<std::uint32_t>(width);
    const auto h = static_cast<std::uint32_t>(height);
    if (w > font.atlasSize || h > font.atlasSize) return false;

    if (font.packCursorX + w > font.atlasSize) {
        font.packCursorX = 0;
        font.packCursorY += font.packRowHeight;
        font.packRowHeight = 0;
    }
    if (font.packCursorY + h > font.atlasSize) {
        return false;
    }
    outX = font.packCursorX;
    outY = font.packCursorY;
    font.packCursorX += w;
    font.packRowHeight = std::max(font.packRowHeight, h);
    return true;
}

/// @brief アトラスを2倍に拡張する（既存のベイク結果は全て破棄し、次回アクセス時に再ベイクさせる）
void GrowAtlas(FontEntry &font) {
    font.atlasSize *= 2;
    font.atlasPixels.assign(static_cast<size_t>(font.atlasSize) * font.atlasSize, 0);
    font.packCursorX = 0;
    font.packCursorY = 0;
    font.packRowHeight = 0;
    font.glyphs.clear();
    font.hasSolidGlyph = false;
}

} // namespace

FontManager::FontManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath)
    : directXCommon_(directXCommon), assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    sAssetsRootPath = assetsRootPath_;
    sDirectXCommon = directXCommon_;
    if (directXCommon_) {
        sDevice = directXCommon_->GetDeviceForFontManager(Passkey<FontManager>{});
    }
    sFonts.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sNameToHandle.clear();
    sNextHandle = 1;
    LoadAllFromAssetsFolder();
}

FontManager::~FontManager() {
    LogScope scope;
    sFonts.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sNameToHandle.clear();
    sDevice = nullptr;
    sDirectXCommon = nullptr;
}

void FontManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(sAssetsRootPath, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir, { ".ttf", ".otf" });

    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &f : d.files) files.push_back(f);
        for (const auto &sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto &f : files) {
        LoadFont(f);
    }
}

FontManager::FontHandle FontManager::LoadFont(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.font.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedFontExtension(p)) {
        Log(Translation("engine.font.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const std::string full = NormalizePathSlashes(PathToUtf8String(p));
    const std::string asset = MakeAssetRelativePath(sAssetsRootPath, full);

    // 既に読み込み済みかチェック
    {
        auto it = sAssetPathToHandle.find(asset);
        if (it != sAssetPathToHandle.end()) return it->second;
    }

    RawFileData raw = LoadFile(full);
    if (raw.data.empty()) {
        Log(Translation("engine.font.loading.failed.read") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    FontEntry entry;
    entry.fileData.assign(raw.data.begin(), raw.data.end());

    auto info = std::make_unique<stbtt_fontinfo>();
    const int offset = stbtt_GetFontOffsetForIndex(entry.fileData.data(), 0);
    if (offset < 0 || !stbtt_InitFont(info.get(), entry.fileData.data(), offset)) {
        Log(Translation("engine.font.loading.failed.parse") + PathToUtf8String(p), LogSeverity::Error);
        return kInvalidHandle;
    }

    entry.fullPath = full;
    entry.assetPath = asset;
    entry.fileName = PathToUtf8String(p.filename());
    entry.name = PathToUtf8String(p.stem());
    entry.info = std::move(info);
    entry.atlasSize = 512;
    entry.atlasPixels.assign(static_cast<size_t>(entry.atlasSize) * entry.atlasSize, 0);

    const FontHandle handle = sNextHandle++;
    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[entry.assetPath] = handle;
    sNameToHandle[entry.name] = handle;
    sFonts.emplace(handle, std::move(entry));

    Log(Translation("engine.font.loading.succeeded") + PathToUtf8String(p), LogSeverity::Info);
    return handle;
}

FontManager::FontHandle FontManager::GetFontHandleFromFileName(const std::string &fileName) {
    auto it = sFileNameToHandle.find(fileName);
    return it != sFileNameToHandle.end() ? it->second : kInvalidHandle;
}

FontManager::FontHandle FontManager::GetFontHandleFromAssetPath(const std::string &assetPath) {
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    return it != sAssetPathToHandle.end() ? it->second : kInvalidHandle;
}

FontManager::FontHandle FontManager::GetFontHandleFromName(const std::string &name) {
    auto it = sNameToHandle.find(name);
    return it != sNameToHandle.end() ? it->second : kInvalidHandle;
}

std::vector<FontManager::FontListEntry> FontManager::GetLoadedFontListEntries() {
    std::vector<FontListEntry> result;
    result.reserve(sFonts.size());
    for (const auto &[handle, entry] : sFonts) {
        result.push_back(FontListEntry{ handle, entry.fileName, entry.assetPath, entry.name });
    }
    return result;
}

void FontManager::UploadAtlasToGpu(FontHandle handle) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end()) return;
    FontEntry &font = it->second;
    if (!sDevice || !sDirectXCommon) return;

    auto newTexture = std::make_unique<ShaderResourceResource>(
        font.atlasSize, font.atlasSize, DXGI_FORMAT_R8_UNORM,
        D3D12_RESOURCE_FLAG_NONE, nullptr, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!newTexture->GetResource()) return;

    D3D12_RESOURCE_DESC texDesc = newTexture->GetResource()->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 requiredSize = 0;
    sDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows, &rowSize, &requiredSize);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = requiredSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc = { 1, 0 };
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    HRESULT hr = sDevice->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upload.GetAddressOf()));
    if (FAILED(hr)) {
        Log(Translation("engine.font.atlas.failed.createupload"), LogSeverity::Error);
        return;
    }

    {
        void *mapped = nullptr;
        D3D12_RANGE range{ 0, 0 };
        hr = upload->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) {
            Log(Translation("engine.font.atlas.failed.map"), LogSeverity::Error);
            return;
        }
        auto *dst = static_cast<uint8_t *>(mapped);
        for (UINT y = 0; y < numRows; ++y) {
            std::memcpy(dst + layout.Offset + static_cast<size_t>(y) * layout.Footprint.RowPitch,
                font.atlasPixels.data() + static_cast<size_t>(y) * font.atlasSize,
                font.atlasSize);
        }
        upload->Unmap(0, nullptr);
    }

    ID3D12Resource *dstResource = newTexture->GetResource();
    sDirectXCommon->ExecuteOneShotCommandsForFontManager(Passkey<FontManager>{},
        [&](ID3D12GraphicsCommandList *cl) {
            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = dstResource;
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = upload.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = layout;

            cl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = dstResource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cl->ResourceBarrier(1, &barrier);
        });

    // テクスチャの実体だけ差し替える（AtlasTextureViewは毎回owner_->atlasTextureを参照するため
    // TextureManagerへの登録・ハンドルはグリフ追加のたびに変わらず安定する。ScreenBufferの
    // ダブルバッファ切り替えと同じ考え方）
    font.atlasTexture = std::move(newTexture);
    if (!font.atlasView) {
        font.atlasView = std::make_unique<AtlasTextureView>(&font);
    }
    if (font.atlasTextureHandle == TextureManager::kInvalidHandle) {
        // assetPath（Assetsルートからの相対パス）はFontManager内で一意なことが保証されているため、
        // 同名フォントファイルが別フォルダに存在してもTextureManager側の登録名衝突を避けられる
        font.atlasTextureHandle = TextureManager::RegisterExternalTexture(font.assetPath + "_GlyphAtlas", font.atlasView.get());
    }
}

const FontManager::GlyphInfo *FontManager::GetOrBakeGlyph(FontHandle handle, char32_t codepoint) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || !it->second.info) return nullptr;
    FontEntry &font = it->second;

    if (auto existing = font.glyphs.find(codepoint); existing != font.glyphs.end()) {
        return &existing->second;
    }

    const int glyphIndex = stbtt_FindGlyphIndex(font.info.get(), static_cast<int>(codepoint));
    if (glyphIndex == 0) {
        // このフォントに存在しない文字：無効なGlyphInfoをキャッシュして毎回の検索を避ける
        auto [inserted, ok] = font.glyphs.emplace(codepoint, GlyphInfo{});
        return &inserted->second;
    }

    const float scale = stbtt_ScaleForPixelHeight(font.info.get(), FontManager::kBakePixelHeight);
    int advanceWidthUnits = 0, leftBearingUnits = 0;
    stbtt_GetGlyphHMetrics(font.info.get(), glyphIndex, &advanceWidthUnits, &leftBearingUnits);

    const float pixelDistScale = 128.0f / static_cast<float>(kSdfPadding);
    int w = 0, h = 0, xoff = 0, yoff = 0;
    unsigned char *bitmap = stbtt_GetGlyphSDF(font.info.get(), scale, glyphIndex,
        kSdfPadding, kSdfOnEdgeValue, pixelDistScale, &w, &h, &xoff, &yoff);
    if (!bitmap || w <= 0 || h <= 0) {
        if (bitmap) stbtt_FreeSDF(bitmap, nullptr);
        GlyphInfo info{};
        info.advance = static_cast<float>(advanceWidthUnits) * scale;
        info.isValid = false;
        auto [inserted, ok] = font.glyphs.emplace(codepoint, info);
        return &inserted->second;
    }

    std::uint32_t px = 0, py = 0;
    while (!PackRect(font, w, h, px, py)) {
        if (font.atlasSize >= kMaxAtlasSize) {
            stbtt_FreeSDF(bitmap, nullptr);
            return nullptr;
        }
        GrowAtlas(font);
    }

    for (int y = 0; y < h; ++y) {
        std::memcpy(&font.atlasPixels[(py + static_cast<std::uint32_t>(y)) * font.atlasSize + px],
            bitmap + static_cast<size_t>(y) * w, static_cast<size_t>(w));
    }
    stbtt_FreeSDF(bitmap, nullptr);

    GlyphInfo info;
    info.u0 = static_cast<float>(px) / static_cast<float>(font.atlasSize);
    info.v0 = static_cast<float>(py) / static_cast<float>(font.atlasSize);
    info.u1 = static_cast<float>(px + static_cast<std::uint32_t>(w)) / static_cast<float>(font.atlasSize);
    info.v1 = static_cast<float>(py + static_cast<std::uint32_t>(h)) / static_cast<float>(font.atlasSize);
    info.width = static_cast<float>(w);
    info.height = static_cast<float>(h);
    info.xoff = static_cast<float>(xoff);
    info.yoff = static_cast<float>(yoff);
    info.advance = static_cast<float>(advanceWidthUnits) * scale;
    info.isValid = true;

    UploadAtlasToGpu(handle);

    auto result = font.glyphs.insert_or_assign(codepoint, info);
    return &result.first->second;
}

const FontManager::GlyphInfo *FontManager::GetSolidGlyph(FontHandle handle) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end()) return nullptr;
    FontEntry &font = it->second;
    if (font.hasSolidGlyph) return &font.solidGlyph;

    std::uint32_t px = 0, py = 0;
    while (!PackRect(font, static_cast<int>(kSolidGlyphSize), static_cast<int>(kSolidGlyphSize), px, py)) {
        if (font.atlasSize >= kMaxAtlasSize) return nullptr;
        GrowAtlas(font);
    }
    for (std::uint32_t y = 0; y < kSolidGlyphSize; ++y) {
        std::memset(&font.atlasPixels[(py + y) * font.atlasSize + px], 0xFF, kSolidGlyphSize);
    }

    GlyphInfo info;
    info.u0 = static_cast<float>(px) / static_cast<float>(font.atlasSize);
    info.v0 = static_cast<float>(py) / static_cast<float>(font.atlasSize);
    info.u1 = static_cast<float>(px + kSolidGlyphSize) / static_cast<float>(font.atlasSize);
    info.v1 = static_cast<float>(py + kSolidGlyphSize) / static_cast<float>(font.atlasSize);
    info.width = static_cast<float>(kSolidGlyphSize);
    info.height = static_cast<float>(kSolidGlyphSize);
    info.xoff = 0.0f;
    info.yoff = 0.0f;
    info.advance = 0.0f;
    info.isValid = true;

    UploadAtlasToGpu(handle);

    font.solidGlyph = info;
    font.hasSolidGlyph = true;
    return &font.solidGlyph;
}

TextureManager::TextureHandle FontManager::GetAtlasTextureHandle(FontHandle handle) {
    auto it = sFonts.find(handle);
    return it != sFonts.end() ? it->second.atlasTextureHandle : TextureManager::kInvalidHandle;
}

float FontManager::GetScaleForPixelHeight(FontHandle handle, float pixelHeight) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || !it->second.info) return 1.0f;
    return stbtt_ScaleForPixelHeight(it->second.info.get(), pixelHeight);
}

float FontManager::GetAscent(FontHandle handle, float pixelHeight) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || !it->second.info) return pixelHeight;
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(it->second.info.get(), &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(it->second.info.get(), pixelHeight);
    return static_cast<float>(ascent) * scale;
}

float FontManager::GetDescent(FontHandle handle, float pixelHeight) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || !it->second.info) return 0.0f;
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(it->second.info.get(), &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(it->second.info.get(), pixelHeight);
    return static_cast<float>(descent) * scale;
}

float FontManager::GetLineHeight(FontHandle handle, float pixelHeight) {
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || !it->second.info) return pixelHeight;
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(it->second.info.get(), &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(it->second.info.get(), pixelHeight);
    return static_cast<float>(ascent - descent + lineGap) * scale;
}

#if defined(USE_IMGUI)
void FontManager::ShowImGuiLoadedFontsWindow() {
    if (!ImGui::Begin(TranslationLabel("editor.fontmanager.window"))) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("FontsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("File");
        ImGui::TableSetupColumn("Atlas Size");
        ImGui::TableSetupColumn("Cached Glyphs");
        ImGui::TableHeadersRow();
        for (const auto &[handle, entry] : sFonts) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.assetPath.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(TranslationC("editor.fontmanager.u_x_u"), entry.atlasSize, entry.atlasSize);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text(TranslationC("editor.fontmanager.desc_1"), entry.glyphs.size());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
#endif

} // namespace KashipanEngine
