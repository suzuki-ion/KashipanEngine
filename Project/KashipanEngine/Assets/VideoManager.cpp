#include "VideoManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>

#include "Assets/CaseInsensitive.h"
#include "Assets/VideoPlayer.h"
#include "Debug/Logger.h"
#include "Graphics/VideoTexture.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Translation.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace KashipanEngine {

namespace {

using VideoHandle = VideoManager::VideoHandle;
using VideoInfo = VideoManager::VideoInfo;

VideoManager *sActiveInstance = nullptr;

std::unordered_map<VideoHandle, VideoInfo> sVideos;
FileMap<VideoHandle> sFileNameToHandle;
FileMap<VideoHandle> sAssetPathToHandle;

std::vector<std::unique_ptr<VideoPlayer>> sPlayers;
// DestroyPlayerで破棄予定になったプレイヤー。ImGui::Image等が今フレーム中に既にSRVハンドルを
// 描画コマンドへ記録している可能性があるため、実体（GPUリソース）の破棄はCommitPendingDestroyまで
// 1フレーム遅延させる（ScreenBuffer::DestroyNotify/CommitDestroyと同じ考え方）
std::vector<std::unique_ptr<VideoPlayer>> sPendingDestroyPlayers;
std::uint32_t sNextPlayerId = 1;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedVideoExtension(const std::filesystem::path &p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".mp4" || ext == ".wmv" || ext == ".mov" || ext == ".avi");
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

/// @brief 動画ストリームの出力形式をNV12に設定する（VideoPlayer::Impl::SetVideoOutputToNV12と同じ手順）
bool SetVideoOutputToNV12(IMFSourceReader *reader) {
    LogScope scope;
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    HRESULT hr = MFCreateMediaType(&type);
    if (FAILED(hr) || !type) return false;
    hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) return false;
    hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (FAILED(hr)) return false;
    hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type.Get());
    return SUCCEEDED(hr);
}

/// @brief 動画デコーダー(MFT)のプロセス内での初回初期化コスト（コーデックDLLロード・
///        ハードウェアデコードセッションのネゴシエーション等）を、実際の再生開始(Play)より前に
///        払っておく。実際にファイルへ1フレーム分デコードを行わせるだけで、結果は破棄する。
/// @details 同一プロセス内で同じコーデックのMFTが一度でも初期化されていれば、以降の
///          IMFSourceReader生成・NV12設定・最初のReadSampleが大幅に高速化される。
///          このため「別のIMFSourceReaderインスタンスを使い捨てで作って1フレーム読み、破棄する」
///          という形でも、再生開始時(VideoPlayer::Play)の初回ReadSampleの体感遅延を軽減できる
void WarmUpVideoDecoder(IMFSourceReader *reader) {
    LogScope scope;
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);
    if (!SetVideoOutputToNV12(reader)) return;

    DWORD flags = 0;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, nullptr, &sample);
    // 結果(sample)は破棄する。この呼び出し自体がMFT初期化のトリガーになることが目的のため、
    // 失敗しても（対応コーデックが無い等）致命的ではないので戻り値は見ない
}

/// @brief 動画ファイルのメタデータ（フレームサイズ・長さ）だけを軽量に取得する
/// @details 実際のフレーム/音声デコードは行わない（ModelManagerと同様、重い処理はPlay時まで遅延する）
bool ProbeVideo(const std::string &filePath, const std::string &assetsRootPath, VideoInfo &outInfo) {
    LogScope scope;
    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.video.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }
    if (!HasSupportedVideoExtension(p)) {
        Log(Translation("engine.video.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    outInfo.fullPath = NormalizePathSlashes(PathToUtf8String(p));
    outInfo.assetPath = MakeAssetRelativePath(assetsRootPath, outInfo.fullPath);
    outInfo.fileName = PathToUtf8String(p.filename());

    const std::wstring wpath(p.wstring());
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) {
        Log(Translation("engine.video.loading.failed.probe") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    Microsoft::WRL::ComPtr<IMFMediaType> nativeType;
    hr = reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &nativeType);
    if (FAILED(hr) || !nativeType) {
        Log(Translation("engine.video.loading.failed.probe") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    UINT32 w = 0, h = 0;
    MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &w, &h);
    outInfo.width = w;
    outInfo.height = h;

    PROPVARIANT durationVar{};
    if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &durationVar))) {
        if (durationVar.vt == VT_UI8) {
            outInfo.durationSec = static_cast<double>(durationVar.uhVal.QuadPart) / 10000000.0;
        }
    }

    if (outInfo.width == 0 || outInfo.height == 0) return false;

    // 起動時のアセットスキャン（このProbeVideo自体の呼び出し）のタイミングで、実際の再生開始(Play)より
    // 前にデコーダーの初回初期化コストを払っておく。このreaderは使い捨てで、直後に破棄してよい
    WarmUpVideoDecoder(reader.Get());

    return true;
}

VideoHandle RegisterEntry(VideoInfo &&entry) {
    const VideoHandle handle = static_cast<VideoHandle>(sVideos.size() + 1u);
    if (handle == VideoManager::kInvalidHandle) return VideoManager::kInvalidHandle;
    if (sVideos.find(handle) != sVideos.end()) return VideoManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sVideos.emplace(handle, std::move(entry));
    return handle;
}

} // namespace

VideoManager::VideoManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)), directXCommon_(directXCommon) {
    LogScope scope;
    sActiveInstance = this;
    InitializeMediaFoundation();
    LoadAllFromAssetsFolder();
}

VideoManager::~VideoManager() {
    LogScope scope;
    sPendingDestroyPlayers.clear();
    sPlayers.clear();
    FinalizeMediaFoundation();
    if (sActiveInstance == this) sActiveInstance = nullptr;
}

void VideoManager::InitializeMediaFoundation() {
    LogScope scope;
    // Media Foundationは内部で参照カウントされ多重初期化が許されているため、
    // AudioManager側の初期化状態には関与せず、VideoManagerが独立してStartup/Shutdownを行う
    const HRESULT hr = MFStartup(MF_VERSION);
    mfInitialized_ = SUCCEEDED(hr);
    if (!mfInitialized_) {
        Log(Translation("engine.video.init.failed.mediafoundation"), LogSeverity::Error);
    }
}

void VideoManager::FinalizeMediaFoundation() {
    LogScope scope;
    if (mfInitialized_) {
        MFShutdown();
        mfInitialized_ = false;
    }
}

void VideoManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);
    const auto filtered = GetDirectoryDataByExtension(dir, { ".mp4", ".wmv", ".mov", ".avi" });

    std::vector<std::string> files;
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &f : d.files) files.push_back(f);
        for (const auto &sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto &file : files) {
        VideoInfo info{};
        if (!ProbeVideo(file, assetsRootPath_, info)) continue;
        const auto handle = RegisterEntry(std::move(info));
        if (handle != kInvalidHandle) {
            Log(Translation("engine.video.loading.succeeded") + file, LogSeverity::Info);
        }
    }
}

VideoManager::VideoHandle VideoManager::Load(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    const std::string normalizedAsset = NormalizePathSlashes(MakeAssetRelativePath(assetsRootPath_, NormalizePathSlashes(PathToUtf8String(Utf8StringToPath(filePath)))));
    {
        auto it = sAssetPathToHandle.find(normalizedAsset);
        if (it != sAssetPathToHandle.end()) return it->second;
    }

    VideoInfo info{};
    if (!ProbeVideo(filePath, assetsRootPath_, info)) return kInvalidHandle;

    const auto handle = RegisterEntry(std::move(info));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.video.loading.failed.register") + filePath, LogSeverity::Error);
    }
    return handle;
}

VideoManager::VideoHandle VideoManager::GetVideoHandleFromFileName(const std::string &fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

VideoManager::VideoHandle VideoManager::GetVideoHandleFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

const VideoManager::VideoInfo *VideoManager::GetVideoInfo(VideoHandle handle) {
    LogScope scope;
    auto it = sVideos.find(handle);
    if (it == sVideos.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> VideoManager::GetLoadedVideoAssetPaths() {
    LogScope scope;
    std::vector<std::string> out;
    out.reserve(sVideos.size());
    for (const auto &kv : sVideos) out.push_back(kv.second.assetPath);
    std::sort(out.begin(), out.end());
    return out;
}

bool VideoManager::RenameVideo(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    auto it = sAssetPathToHandle.find(normalizedOld);
    if (it == sAssetPathToHandle.end()) return false;

    const VideoHandle handle = it->second;
    sAssetPathToHandle.erase(it);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    sAssetPathToHandle[normalizedNew] = handle;

    auto entryIt = sVideos.find(handle);
    if (entryIt != sVideos.end()) {
        entryIt->second.assetPath = normalizedNew;
        const std::filesystem::path p = Utf8StringToPath(newAssetPath);
        const std::string newFileName = PathToUtf8String(p.filename());
        sFileNameToHandle.erase(entryIt->second.fileName);
        entryIt->second.fileName = newFileName;
        sFileNameToHandle[newFileName] = handle;
    }
    return true;
}

#if defined(USE_IMGUI)
VideoManager::VideoHandle VideoManager::LoadDynamic(const std::string &filePath) {
    LogScope scope;
    if (!sActiveInstance) return kInvalidHandle;
    return sActiveInstance->Load(filePath);
}
#endif

VideoPlayer *VideoManager::CreatePlayer(VideoHandle handle) {
    LogScope scope;
    if (!sActiveInstance) return nullptr;
    auto it = sVideos.find(handle);
    if (it == sVideos.end()) return nullptr;

    const std::string prefix = "Video_" + std::to_string(sNextPlayerId++);
    auto player = std::make_unique<VideoPlayer>(sActiveInstance->directXCommon_, it->second.fullPath, prefix);
    VideoPlayer *raw = player.get();
    sPlayers.push_back(std::move(player));
    return raw;
}

void VideoManager::DestroyPlayer(VideoPlayer *player) {
    LogScope scope;
    if (!player) return;
    for (auto it = sPlayers.begin(); it != sPlayers.end(); ++it) {
        if (it->get() == player) {
            // 実体の破棄（音声停止・GPUリソース解放）はCommitPendingDestroyまで1フレーム遅延させる。
            // 破棄後はsPlayersから抜けるためVideoManager::Update()の対象からは即座に外れる
            sPendingDestroyPlayers.push_back(std::move(*it));
            sPlayers.erase(it);
            return;
        }
    }
}

void VideoManager::CommitPendingDestroy(Passkey<GameEngine>) {
    LogScope scope;
    // unique_ptrの破棄でVideoPlayerのデストラクタ(Stop())が走り、ここで初めて
    // 音声停止・GPUリソース（VideoTexture）の実体解放が行われる。この呼び出しは
    // GameLoopDraw完了（ImGuiの描画コマンド発行＋WaitForFenceによるGPU同期）の後で
    // 行うこと（GameEngine::Execute参照）
    sPendingDestroyPlayers.clear();
}

void VideoManager::Update() {
    LogScope scope;
    for (auto &player : sPlayers) {
        player->Update();
    }
}

std::vector<VideoTexture *> VideoManager::GetPendingConversions(Passkey<Renderer> passkey) {
    LogScope scope;
    std::vector<VideoTexture *> result;
    for (auto &player : sPlayers) {
        VideoTexture *videoTexture = player->GetVideoTexture(Passkey<VideoManager>{});
        if (videoTexture && videoTexture->HasPendingConversion(passkey)) {
            result.push_back(videoTexture);
        }
    }
    return result;
}

#if defined(USE_IMGUI)
void VideoManager::ShowImGuiLoadedVideosWindow() {
    if (ImGui::Begin("Loaded Videos")) {
        if (ImGui::BeginTable("LoadedVideosTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Handle");
            ImGui::TableSetupColumn("Asset Path");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Duration(s)");
            ImGui::TableHeadersRow();
            for (const auto &kv : sVideos) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", kv.first);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(kv.second.assetPath.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%ux%u", kv.second.width, kv.second.height);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f", kv.second.durationSec);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void VideoManager::ShowImGuiPlayingVideosWindow() {
    if (ImGui::Begin("Playing Videos")) {
        for (std::size_t i = 0; i < sPlayers.size(); ++i) {
            VideoPlayer *player = sPlayers[i].get();
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s (%ux%u)", player->GetFullPath().c_str(), player->GetWidth(), player->GetHeight());
            ImGui::SameLine();
            if (player->IsPaused()) {
                if (ImGui::SmallButton("Resume")) player->Resume();
            } else {
                if (ImGui::SmallButton("Pause")) player->Pause();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop")) player->Stop();
            ImGui::PopID();
        }
    }
    ImGui::End();
}
#endif

} // namespace KashipanEngine
