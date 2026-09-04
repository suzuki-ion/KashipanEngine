#include "GifManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

#include <wincodec.h>
#include <wrl.h>

#include "Assets/CaseInsensitive.h"
#include "Assets/GifPlayer.h"
#include "Debug/Logger.h"
#include "Graphics/GifTexture.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Plugin/Plugins.h"
#include "Utilities/Translation.h"

#pragma comment(lib, "windowscodecs.lib")

namespace KashipanEngine {

namespace {

using GifHandle = GifManager::GifHandle;
using GifAnimation = GifManager::GifAnimation;
using GifFrame = GifManager::GifFrame;

GifManager *sActiveInstance = nullptr;

std::unordered_map<GifHandle, GifAnimation> sGifs;
FileMap<GifHandle> sFileNameToHandle;
FileMap<GifHandle> sAssetPathToHandle;

// DestroyPlayerで破棄予定になったGifPlayer。ImGui::Image等が今フレーム中に既にSRVハンドルを
// 描画コマンドへ記録している可能性があるため、実体（GifTextureのGPUリソース）の破棄は
// CommitPendingDestroyまで1フレーム遅延させる（ScreenBuffer::DestroyNotify/CommitDestroyと同じ考え方）
std::vector<std::unique_ptr<GifPlayer>> sPendingDestroyPlayers;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasGifExtension(const std::filesystem::path &p) {
    return ToLower(p.extension().string()) == ".gif";
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

/// @brief PROPVARIANTからUI2/UI1値を読み取るヘルパー（型不一致・取得失敗時は既定値を返す）
std::uint32_t ReadPropVariantUInt(IWICMetadataQueryReader *reader, LPCWSTR name, std::uint32_t defaultValue) {
    if (!reader) return defaultValue;
    PROPVARIANT v{};
    if (FAILED(reader->GetMetadataByName(name, &v))) return defaultValue;
    std::uint32_t result = defaultValue;
    if (v.vt == VT_UI2) result = v.uiVal;
    else if (v.vt == VT_UI1) result = v.bVal;
    else if (v.vt == VT_UI4) result = v.ulVal;
    PropVariantClear(&v);
    return result;
}

/// @brief キャンバス上の矩形を透明色でクリアする（GIFのDisposal=2「背景に戻す」の簡易実装）
void ClearRect(std::vector<std::uint8_t> &canvas, std::uint32_t canvasWidth, std::uint32_t canvasHeight,
    std::uint32_t left, std::uint32_t top, std::uint32_t w, std::uint32_t h) {
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint32_t cy = top + y;
        if (cy >= canvasHeight) break;
        std::uint8_t *row = &canvas[(static_cast<std::size_t>(cy) * canvasWidth + left) * 4];
        const std::uint32_t clampedW = std::min<std::uint32_t>(w, canvasWidth - left);
        std::fill(row, row + static_cast<std::size_t>(clampedW) * 4, 0);
    }
}

struct RawGifFrame final {
    std::vector<std::uint8_t> rgba;
    std::uint32_t left = 0, top = 0, width = 0, height = 0;
    float delaySeconds = 0.1f;
    std::uint8_t disposal = 0;
};

/// @brief WICを使いアニメーションGIFの全フレームをデコードし、キャンバス合成した各フレームのRGBAへ変換する
bool DecodeGifAnimation(const std::string &filePath, const std::string &assetsRootPath, GifAnimation &out) {
    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.gif.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }
    if (!HasGifExtension(p)) {
        Log(Translation("engine.gif.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    const std::wstring wpath = ConvertString(PathToUtf8String(p));
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    UINT frameCount = 0;
    decoder->GetFrameCount(&frameCount);
    if (frameCount == 0) {
        Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    std::uint32_t canvasWidth = 0, canvasHeight = 0;
    Microsoft::WRL::ComPtr<IWICMetadataQueryReader> globalReader;
    if (SUCCEEDED(decoder->GetMetadataQueryReader(&globalReader))) {
        canvasWidth = ReadPropVariantUInt(globalReader.Get(), L"/logscrdesc/Width", 0);
        canvasHeight = ReadPropVariantUInt(globalReader.Get(), L"/logscrdesc/Height", 0);
    }

    std::vector<RawGifFrame> rawFrames(frameCount);
    for (UINT i = 0; i < frameCount; ++i) {
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(i, &frame);
        if (FAILED(hr) || !frame) {
            Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
            return false;
        }

        UINT fw = 0, fh = 0;
        frame->GetSize(&fw, &fh);
        if (fw == 0 || fh == 0) {
            Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
            return false;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr)) {
            hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                nullptr, 0.0, WICBitmapPaletteTypeCustom);
        }
        if (FAILED(hr)) {
            Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
            return false;
        }

        RawGifFrame &rf = rawFrames[i];
        rf.width = fw;
        rf.height = fh;
        rf.rgba.resize(static_cast<std::size_t>(fw) * fh * 4);
        hr = converter->CopyPixels(nullptr, fw * 4, static_cast<UINT>(rf.rgba.size()), rf.rgba.data());
        if (FAILED(hr)) {
            Log(Translation("engine.gif.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
            return false;
        }

        Microsoft::WRL::ComPtr<IWICMetadataQueryReader> frameReader;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&frameReader))) {
            rf.left = ReadPropVariantUInt(frameReader.Get(), L"/imgdesc/Left", 0);
            rf.top = ReadPropVariantUInt(frameReader.Get(), L"/imgdesc/Top", 0);
            const std::uint32_t delayCentiseconds = ReadPropVariantUInt(frameReader.Get(), L"/grctlext/Delay", 0);
            rf.delaySeconds = (delayCentiseconds > 0) ? (static_cast<float>(delayCentiseconds) / 100.0f) : 0.1f;
            rf.disposal = static_cast<std::uint8_t>(ReadPropVariantUInt(frameReader.Get(), L"/grctlext/Disposal", 0));
        }

        if (canvasWidth == 0 || canvasHeight == 0) {
            // ロジカルスクリーン記述子が取得できなかった場合、フレーム自身のサイズをキャンバスとして扱う
            canvasWidth = std::max(canvasWidth, rf.left + rf.width);
            canvasHeight = std::max(canvasHeight, rf.top + rf.height);
        }
    }

    out.width = canvasWidth;
    out.height = canvasHeight;
    out.frames.resize(frameCount);

    // キャンバス合成: 各フレームは直前フレームのDisposalに従ってキャンバスを更新した上で、
    // 自身のRGBAをalpha!=0のピクセルだけ上書きする（GIFの透過は1bitマスクでありアルファブレンドではない）。
    // Disposal=2（背景に戻す）は本来GIFの背景色を使うべきだが、多くのビューアと同様に透明へ戻す近似で扱う
    std::vector<std::uint8_t> canvas(static_cast<std::size_t>(canvasWidth) * canvasHeight * 4, 0);
    std::vector<std::uint8_t> canvasBackup;
    bool hasBackup = false;
    bool hasPrevious = false;
    std::uint8_t previousDisposal = 0;
    std::uint32_t prevLeft = 0, prevTop = 0, prevW = 0, prevH = 0;

    for (UINT i = 0; i < frameCount; ++i) {
        if (hasPrevious) {
            if (previousDisposal == 2) {
                ClearRect(canvas, canvasWidth, canvasHeight, prevLeft, prevTop, prevW, prevH);
            } else if (previousDisposal == 3 && hasBackup) {
                canvas = canvasBackup;
            }
        }

        const RawGifFrame &rf = rawFrames[i];
        if (rf.disposal == 3) {
            canvasBackup = canvas;
            hasBackup = true;
        }

        for (std::uint32_t y = 0; y < rf.height; ++y) {
            const std::uint32_t cy = rf.top + y;
            if (cy >= canvasHeight) break;
            for (std::uint32_t x = 0; x < rf.width; ++x) {
                const std::uint32_t cx = rf.left + x;
                if (cx >= canvasWidth) continue;
                const std::uint8_t *src = &rf.rgba[(static_cast<std::size_t>(y) * rf.width + x) * 4];
                if (src[3] == 0) continue;
                std::uint8_t *dst = &canvas[(static_cast<std::size_t>(cy) * canvasWidth + cx) * 4];
                dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
            }
        }

        out.frames[i].rgba = canvas;
        out.frames[i].delaySeconds = rf.delaySeconds;

        previousDisposal = rf.disposal;
        prevLeft = rf.left; prevTop = rf.top; prevW = rf.width; prevH = rf.height;
        hasPrevious = true;
    }

    out.fullPath = NormalizePathSlashes(PathToUtf8String(p));
    out.assetPath = MakeAssetRelativePath(assetsRootPath, out.fullPath);
    out.fileName = PathToUtf8String(p.filename());
    return true;
}

GifHandle RegisterEntry(GifAnimation &&entry) {
    const GifHandle handle = static_cast<GifHandle>(sGifs.size() + 1u);
    if (handle == GifManager::kInvalidHandle) return GifManager::kInvalidHandle;
    if (sGifs.find(handle) != sGifs.end()) return GifManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sGifs.emplace(handle, std::move(entry));
    return handle;
}

} // namespace

GifManager::GifManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)), directXCommon_(directXCommon) {
    LogScope scope;
    sActiveInstance = this;
    LoadAllFromAssetsFolder();
}

GifManager::~GifManager() {
    LogScope scope;
    sPendingDestroyPlayers.clear();
    if (sActiveInstance == this) sActiveInstance = nullptr;
}

void GifManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);
    const auto filtered = GetDirectoryDataByExtension(dir, { ".gif" });

    std::vector<std::string> files;
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &f : d.files) files.push_back(f);
        for (const auto &sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    // デコードはCPUのみでGPUリソースに触れないため、TextureManagerと同様にスレッドプールで並列実行する
    std::vector<GifAnimation> decoded(files.size());
    std::vector<bool> succeeded(files.size(), false);
    Plugin::RunParallelAndWait(files.size(), [&](size_t i) {
        succeeded[i] = DecodeGifAnimation(files[i], assetsRootPath_, decoded[i]);
    });

    for (size_t i = 0; i < files.size(); ++i) {
        if (!succeeded[i]) continue;
        const auto handle = RegisterEntry(std::move(decoded[i]));
        if (handle != kInvalidHandle) {
            Log(Translation("engine.gif.loading.succeeded") + files[i], LogSeverity::Info);
        }
    }
}

GifManager::GifHandle GifManager::Load(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    const std::string normalizedAsset = NormalizePathSlashes(MakeAssetRelativePath(assetsRootPath_, NormalizePathSlashes(PathToUtf8String(Utf8StringToPath(filePath)))));
    {
        auto it = sAssetPathToHandle.find(normalizedAsset);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.gif.loading.alreadyloaded") + filePath, LogSeverity::Debug);
            return it->second;
        }
    }

    GifAnimation animation{};
    if (!DecodeGifAnimation(filePath, assetsRootPath_, animation)) return kInvalidHandle;

    const auto handle = RegisterEntry(std::move(animation));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.gif.loading.failed.register") + filePath, LogSeverity::Error);
    }
    return handle;
}

GifManager::GifHandle GifManager::GetGifHandleFromFileName(const std::string &fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

GifManager::GifHandle GifManager::GetGifHandleFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

const GifManager::GifAnimation *GifManager::GetGifAnimation(GifHandle handle) {
    LogScope scope;
    auto it = sGifs.find(handle);
    if (it == sGifs.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> GifManager::GetLoadedGifAssetPaths() {
    LogScope scope;
    std::vector<std::string> out;
    out.reserve(sGifs.size());
    for (const auto &kv : sGifs) out.push_back(kv.second.assetPath);
    std::sort(out.begin(), out.end());
    return out;
}

bool GifManager::RenameGif(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    auto it = sAssetPathToHandle.find(normalizedOld);
    if (it == sAssetPathToHandle.end()) return false;

    const GifHandle handle = it->second;
    sAssetPathToHandle.erase(it);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    sAssetPathToHandle[normalizedNew] = handle;

    auto entryIt = sGifs.find(handle);
    if (entryIt != sGifs.end()) {
        entryIt->second.assetPath = normalizedNew;
        const std::filesystem::path p = Utf8StringToPath(newAssetPath);
        const std::string newFileName = PathToUtf8String(p.filename());
        sFileNameToHandle.erase(entryIt->second.fileName);
        entryIt->second.fileName = newFileName;
        sFileNameToHandle[newFileName] = handle;
    }
    return true;
}

std::unique_ptr<GifPlayer> GifManager::CreatePlayer(GifHandle handle) {
    LogScope scope;
    if (!sActiveInstance) return nullptr;
    if (sGifs.find(handle) == sGifs.end()) return nullptr;
    return std::make_unique<GifPlayer>(sActiveInstance->directXCommon_, handle);
}

std::unique_ptr<GifTexture> GifManager::CreateGifTexture(Passkey<GifPlayer>, GifHandle handle) {
    LogScope scope;
    if (!sActiveInstance) return nullptr;
    auto it = sGifs.find(handle);
    if (it == sGifs.end()) return nullptr;

    auto texture = std::make_unique<GifTexture>(sActiveInstance->directXCommon_, it->second.width, it->second.height);
    if (!texture->IsValid()) return nullptr;
    return texture;
}

void GifManager::DestroyPlayer(std::unique_ptr<GifPlayer> player) {
    LogScope scope;
    if (!player) return;
    sPendingDestroyPlayers.push_back(std::move(player));
}

void GifManager::CommitPendingDestroy(Passkey<GameEngine>) {
    LogScope scope;
    // unique_ptrの破棄でGifPlayerのデストラクタが走り、ここで初めてGifTextureのGPUリソース
    // （SRVを含む）の実体解放が行われる。この呼び出しはGameLoopDraw完了（ImGuiの描画コマンド
    // 発行＋GPU同期）の後で行うこと（GameEngine::Execute参照）
    sPendingDestroyPlayers.clear();
}

#if defined(USE_IMGUI)
GifManager::GifHandle GifManager::LoadDynamic(const std::string &filePath) {
    LogScope scope;
    if (!sActiveInstance) return kInvalidHandle;
    return sActiveInstance->Load(filePath);
}
#endif

} // namespace KashipanEngine
