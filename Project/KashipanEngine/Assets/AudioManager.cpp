#include "AudioManager.h"
#include "Assets/CaseInsensitive.h"
#include "Core/ProjectPaths.h"
#include "Assets/AudioPlayer.h"
#include "Assets/SoundBeat.h"

#include "Debug/Logger.h"
#include "Utilities/Translation.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Plugin/Plugins.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_internal.h>
#endif

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <xaudio2.h>
#include <xaudio2fx.h>
#include <xapofx.h>
#include <wrl.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace KashipanEngine {

namespace {

using SoundHandle = AudioManager::SoundHandle;
using PlayHandle = AudioManager::PlayHandle;

AudioManager* sActiveInstance = nullptr;

struct SoundEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    WAVEFORMATEX wfex{};
    std::vector<BYTE> buffer;
};

struct PlayEntry final {
    SoundHandle sound = AudioManager::kInvalidSoundHandle;
    IXAudio2SourceVoice* voice = nullptr;
    bool paused = false;
    bool loop = false;
    double startTimeSec = 0.0;
    uint32_t sourceChannels = 0;
    /// @brief XAPOFXのエフェクトチェーン(EQ/エコー/リミッター)付きでボイスを生成できたかどうか
    bool hasEffectChain = false;
    /// @brief 各チェーンエフェクトの現在の有効状態（Enable/DisableEffectの冗長呼び出しを避けるためのキャッシュ）
    bool eqEnabled = false;
    bool echoEnabled = false;
    bool limiterEnabled = false;
};

// ソースボイスのエフェクトチェーン内のスロット番号（CreateSourceVoiceに渡す並び順と一致させること）
constexpr UINT32 kEffectSlotEq = 0;
constexpr UINT32 kEffectSlotEcho = 1;
constexpr UINT32 kEffectSlotLimiter = 2;

XAUDIO2_FILTER_TYPE ToXAudio2FilterType(AudioManager::FilterType type) {
    switch (type) {
    case AudioManager::FilterType::HighPass: return HighPassFilter;
    case AudioManager::FilterType::BandPass: return BandPassFilter;
    case AudioManager::FilterType::Notch:    return NotchFilter;
    case AudioManager::FilterType::LowPass:
    default:                                 return LowPassFilter;
    }
}

std::unordered_map<SoundHandle, SoundEntry> sSounds;
FileMap<SoundHandle> sAssetPathToHandle;
FileMap<SoundHandle> sFileNameToHandle;

Microsoft::WRL::ComPtr<IXAudio2> sXaudio2;
IXAudio2MasteringVoice* sMasterVoice = nullptr;
// 全AudioSource共有のリバーブ送り先(モノ入力の共有バス)。初期化に失敗した場合は nullptr のままとなり、
// リバーブ関連APIはすべて何もせず false を返す
IXAudio2SubmixVoice* sReverbSubmixVoice = nullptr;
Microsoft::WRL::ComPtr<IUnknown> sReverbEffect;
bool sMfStarted = false;

static constexpr size_t kMaxSimultaneousPlays = 64;
std::vector<std::unique_ptr<PlayEntry>> sPlays;
std::unordered_set<size_t> sUsedPlayIndices;
std::vector<size_t> sFreePlayIndices;

std::unordered_map<PlayHandle, size_t> sPlayHandleToIndex;
std::unordered_set<PlayHandle> sUsedPlayHandles;

std::unordered_set<SoundBeat*> sRegisteredSoundBeats;
std::unordered_set<AudioPlayer*> sRegisteredAudioPlayers;

std::mt19937& Rng() {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    return rng;
}

PlayHandle GenerateUniquePlayHandle() {
    std::uniform_int_distribution<uint32_t> dist(1u, 0xFFFFFFFFu);
    for (int i = 0; i < 128; ++i) {
        const PlayHandle h = static_cast<PlayHandle>(dist(Rng()));
        if (h != AudioManager::kInvalidPlayHandle && sUsedPlayHandles.find(h) == sUsedPlayHandles.end()) {
            return h;
        }
    }

    static uint64_t counter = 1;
    while (true) {
        uint64_t x = ++counter;
        x ^= (x << 13);
        x ^= (x >> 7);
        x ^= (x << 17);
        const PlayHandle h = static_cast<PlayHandle>((x & 0xFFFFFFFFu) ? (x & 0xFFFFFFFFu) : 1u);
        if (h != AudioManager::kInvalidPlayHandle && sUsedPlayHandles.find(h) == sUsedPlayHandles.end()) {
            return h;
        }
    }
}

void ReleasePlayHandle(PlayHandle h) {
    if (h == AudioManager::kInvalidPlayHandle) return;
    sUsedPlayHandles.erase(h);
    sPlayHandleToIndex.erase(h);
}

bool TryGetPlayIndex(PlayHandle h, size_t& outIdx) {
    if (h == AudioManager::kInvalidPlayHandle) return false;
    auto it = sPlayHandleToIndex.find(h);
    if (it == sPlayHandleToIndex.end()) return false;
    outIdx = it->second;
    return true;
}

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedAudioExtension(const std::filesystem::path& p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aac" || ext == ".m4a" || ext == ".wma");
}

std::string MakeAssetRelativePath(const std::string& assetsRoot, const std::string& fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

SoundHandle RegisterEntry(SoundEntry&& entry) {
    const SoundHandle handle = static_cast<SoundHandle>(sSounds.size() + 1u);
    if (handle == AudioManager::kInvalidSoundHandle) return AudioManager::kInvalidSoundHandle;
    if (sSounds.find(handle) != sSounds.end()) return AudioManager::kInvalidSoundHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sSounds.emplace(handle, std::move(entry));
    return handle;
}

size_t AcquirePlayIndex() {
    if (!sFreePlayIndices.empty()) {
        size_t idx = sFreePlayIndices.back();
        sFreePlayIndices.pop_back();
        return idx;
    }

    for (size_t i = 0; i < sPlays.size(); ++i) {
        if (sUsedPlayIndices.find(i) == sUsedPlayIndices.end()) {
            return i;
        }
    }

    if (sPlays.size() < kMaxSimultaneousPlays) {
        sPlays.emplace_back(std::make_unique<PlayEntry>());
        return sPlays.size() - 1;
    }

    return static_cast<size_t>(-1);
}

void ReleasePlayIndex(size_t idx) {
    if (idx == static_cast<size_t>(-1)) return;
    sUsedPlayIndices.erase(idx);
    sFreePlayIndices.push_back(idx);
}

void StopVoice(PlayEntry& p) {
    if (!p.voice) return;
    p.voice->Stop();
    p.voice->FlushSourceBuffers();
    p.voice->DestroyVoice();
    p.voice = nullptr;
    p.sound = AudioManager::kInvalidSoundHandle;
    p.paused = false;
    p.startTimeSec = 0.0;
    p.sourceChannels = 0;
}

bool EnsureAudioInitialized() {
    if (sXaudio2) return true;

    LogScope scope;

    HRESULT hr = XAudio2Create(&sXaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || !sXaudio2) {
        Log(Translation("engine.audio.init.failed.xaudio2"), LogSeverity::Error);
        return false;
    }

    hr = sXaudio2->CreateMasteringVoice(&sMasterVoice);
    if (FAILED(hr) || !sMasterVoice) {
        Log(Translation("engine.audio.init.failed.mastervoice"), LogSeverity::Error);
        sXaudio2.Reset();
        return false;
    }

    {
        // 全AudioSource共有のリバーブ送り先バス(モノ入力)を作成する。失敗してもオーディオ全体の初期化は継続する
        Microsoft::WRL::ComPtr<IUnknown> reverbEffect;
        HRESULT reverbHr = CreateAudioReverb(&reverbEffect);
        if (SUCCEEDED(reverbHr) && reverbEffect) {
            XAUDIO2_EFFECT_DESCRIPTOR effectDescriptor{};
            effectDescriptor.InitialState = TRUE;
            effectDescriptor.OutputChannels = 1;
            effectDescriptor.pEffect = reverbEffect.Get();
            XAUDIO2_EFFECT_CHAIN effectChain{ 1, &effectDescriptor };

            XAUDIO2_VOICE_DETAILS masterDetails{};
            sMasterVoice->GetVoiceDetails(&masterDetails);

            reverbHr = sXaudio2->CreateSubmixVoice(&sReverbSubmixVoice, 1, masterDetails.InputSampleRate, 0, 0, nullptr, &effectChain);
            if (SUCCEEDED(reverbHr) && sReverbSubmixVoice) {
                sReverbEffect = reverbEffect;
                XAUDIO2FX_REVERB_I3DL2_PARAMETERS i3dl2Preset = XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM;
                XAUDIO2FX_REVERB_PARAMETERS reverbParams{};
                ReverbConvertI3DL2ToNative(&i3dl2Preset, &reverbParams);
                sReverbSubmixVoice->SetEffectParameters(0, &reverbParams, sizeof(reverbParams));
            } else {
                sReverbSubmixVoice = nullptr;
                Log(Translation("engine.audio.init.failed.reverb"), LogSeverity::Warning);
            }
        } else {
            Log(Translation("engine.audio.init.failed.reverb"), LogSeverity::Warning);
        }
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        Log(Translation("engine.audio.init.failed.mediafoundation"), LogSeverity::Error);
        sMasterVoice->DestroyVoice();
        sMasterVoice = nullptr;
        sXaudio2.Reset();
        return false;
    }

    sMfStarted = true;

    sPlays.clear();
    sPlays.reserve(kMaxSimultaneousPlays);
    sUsedPlayIndices.clear();
    sFreePlayIndices.clear();
    sPlayHandleToIndex.clear();
    sUsedPlayHandles.clear();

    return true;
}

void FinalizeAudio() {
    LogScope scope;

    for (size_t idx : sUsedPlayIndices) {
        if (idx < sPlays.size() && sPlays[idx]) {
            StopVoice(*sPlays[idx]);
        }
    }

    sUsedPlayIndices.clear();
    sFreePlayIndices.clear();
    sPlays.clear();
    sPlayHandleToIndex.clear();
    sUsedPlayHandles.clear();

    if (sMfStarted) {
        MFShutdown();
        sMfStarted = false;
    }

    if (sReverbSubmixVoice) {
        sReverbSubmixVoice->DestroyVoice();
        sReverbSubmixVoice = nullptr;
    }
    sReverbEffect.Reset();

    if (sMasterVoice) {
        sMasterVoice->DestroyVoice();
        sMasterVoice = nullptr;
    }

    sXaudio2.Reset();
}

bool DecodeToPcm(const std::wstring& wpath, WAVEFORMATEX& outWfex, std::vector<BYTE>& outBuffer) {
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) return false;

    Microsoft::WRL::ComPtr<IMFMediaType> type;
    hr = MFCreateMediaType(&type);
    if (FAILED(hr) || !type) return false;

    hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) return false;

    hr = type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) return false;

    hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type.Get());
    if (FAILED(hr)) return false;

    type.Reset();
    hr = reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &type);
    if (FAILED(hr) || !type) return false;

    WAVEFORMATEX* wf = nullptr;
    hr = MFCreateWaveFormatExFromMFMediaType(type.Get(), &wf, nullptr);
    if (FAILED(hr) || !wf) return false;

    outWfex = *wf;
    CoTaskMemFree(wf);

    outBuffer.clear();

    while (true) {
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD flags = 0;
        hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) return false;

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
        hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
        if (FAILED(hr) || !mediaBuffer) return false;

        BYTE* data = nullptr;
        DWORD curLen = 0;
        hr = mediaBuffer->Lock(&data, nullptr, &curLen);
        if (FAILED(hr) || !data || curLen == 0) {
            mediaBuffer->Unlock();
            return false;
        }

        const size_t oldSize = outBuffer.size();
        outBuffer.resize(oldSize + curLen);
        std::memcpy(outBuffer.data() + oldSize, data, curLen);

        mediaBuffer->Unlock();
    }

    return !outBuffer.empty();
}

/// @brief ファイルをデコードしてSoundEntryを組み立てる（グローバル状態への登録は行わない）
/// @details ファイルI/O・Media Foundationによるデコードのみを行うため、スレッドプールから
///          並列に呼び出しても安全（各呼び出しは自分のIMFSourceReaderを持ち、outEntryも呼び出し元専有）
bool DecodeAudioFile(const std::string& filePath, const std::string& assetsRootPath, SoundEntry& outEntry) {
    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.audio.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }
    if (!HasSupportedAudioExtension(p)) {
        Log(Translation("engine.audio.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }

    outEntry.fullPath = NormalizePathSlashes(PathToUtf8String(p));
    outEntry.assetPath = MakeAssetRelativePath(assetsRootPath, outEntry.fullPath);
    outEntry.fileName = PathToUtf8String(p.filename());

    const std::wstring wpath(p.wstring());
    if (!DecodeToPcm(wpath, outEntry.wfex, outEntry.buffer)) {
        Log(Translation("engine.audio.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
        return false;
    }
    return true;
}

float SemitonesToFrequencyRatio(float semitones) {
    return std::pow(2.0f, semitones / 12.0f);
}

bool IsVoiceActuallyPlaying(IXAudio2SourceVoice* voice) {
    if (!voice) return false;
    XAUDIO2_VOICE_STATE state{};
    voice->GetState(&state);
    return state.BuffersQueued > 0;
}

uint32_t EstimateDurationMs(const WAVEFORMATEX& wfex, const std::vector<BYTE>& buffer) {
    if (wfex.nAvgBytesPerSec == 0) return 0;
    const double seconds = static_cast<double>(buffer.size()) / static_cast<double>(wfex.nAvgBytesPerSec);
    const double ms = seconds * 1000.0;
    if (ms <= 0.0) return 0;
    if (ms > static_cast<double>(std::numeric_limits<uint32_t>::max())) return std::numeric_limits<uint32_t>::max();
    return static_cast<uint32_t>(ms);
}

} // namespace

bool AudioManager::GetPlayPositionSeconds(PlayHandle play, double& outSeconds) {
    outSeconds = 0.0;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;

    const PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;
    if (p.sound == kInvalidSoundHandle) return false;

    XAUDIO2_VOICE_STATE state{};
    p.voice->GetState(&state);

    const auto it = sSounds.find(p.sound);
    if (it == sSounds.end()) return false;
    const WAVEFORMATEX& wf = it->second.wfex;
    const uint32_t samplesPerSec = static_cast<uint32_t>(wf.nSamplesPerSec);
    if (samplesPerSec == 0) return false;

    const uint64_t samplesPlayed = state.SamplesPlayed; // フレーム（サンプルフレーム）
    outSeconds = static_cast<double>(samplesPlayed) / static_cast<double>(samplesPerSec);
    outSeconds += p.startTimeSec;
    return true;
}

AudioManager::AudioManager(Passkey<GameEngine>, const std::string& assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    sActiveInstance = this;
    InitializeAudioDevice();
    LoadAllFromAssetsFolder();
}

AudioManager::~AudioManager() {
    LogScope scope;

    if (sActiveInstance == this) sActiveInstance = nullptr;
    FinalizeAudioDevice();
    sSounds.clear();
    sAssetPathToHandle.clear();
    sFileNameToHandle.clear();
}

void AudioManager::InitializeAudioDevice() {
    EnsureAudioInitialized();
}

void AudioManager::FinalizeAudioDevice() {
    FinalizeAudio();
}

void AudioManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir,
        { ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma" });

    std::function<void(const DirectoryData&)> flatten = [&](const DirectoryData& d) {
        for (const auto& f : d.files) files.push_back(f);
        for (const auto& sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    // ファイルI/O・デコード(Media Foundation)はCPU処理のみでグローバル状態に触れないため、
    // スレッドプールで並列実行する。各要素は担当するインデックス以外書き込まないためロック不要
    std::vector<SoundEntry> decodedEntries(files.size());
    std::vector<bool> decodedOk(files.size(), false);
    Plugin::RunParallelAndWait(files.size(), [this, &files, &decodedEntries, &decodedOk](size_t i) {
        decodedOk[i] = DecodeAudioFile(files[i], assetsRootPath_, decodedEntries[i]);
        });

    // 全ファイルのデコードが完了したら、メインスレッドでグローバルマップへの登録を順に行う
    for (size_t i = 0; i < files.size(); ++i) {
        if (!decodedOk[i]) continue;
        const auto handle = RegisterEntry(std::move(decodedEntries[i]));
        if (handle == kInvalidSoundHandle) {
            Log(Translation("engine.audio.loading.failed.register") + files[i], LogSeverity::Error);
            continue;
        }
        Log(Translation("engine.audio.loading.succeeded") + files[i], LogSeverity::Info);
    }
}

SoundHandle AudioManager::Load(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidSoundHandle;

    if (!EnsureAudioInitialized()) return kInvalidSoundHandle;

    const std::string normalizedAsset = NormalizePathSlashes(MakeAssetRelativePath(assetsRootPath_, NormalizePathSlashes(PathToUtf8String(Utf8StringToPath(filePath)))));
    {
        auto it = sAssetPathToHandle.find(normalizedAsset);
        if (it != sAssetPathToHandle.end()) return it->second;
    }

    SoundEntry entry{};
    if (!DecodeAudioFile(filePath, assetsRootPath_, entry)) {
        return kInvalidSoundHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidSoundHandle) {
        Log(Translation("engine.audio.loading.failed.register") + filePath, LogSeverity::Error);
        return kInvalidSoundHandle;
    }

    Log(Translation("engine.audio.loading.succeeded") + filePath, LogSeverity::Info);
    return handle;
}

#if defined(USE_IMGUI)
SoundHandle AudioManager::LoadDynamic(const std::string &filePath) {
    if (!sActiveInstance) return kInvalidSoundHandle;
    return sActiveInstance->Load(filePath);
}
#endif

SoundHandle AudioManager::GetSoundHandleFromFileName(const std::string &fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidSoundHandle;
    return it->second;
}

SoundHandle AudioManager::GetSoundHandleFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidSoundHandle;
    return it->second;
}

std::vector<std::string> AudioManager::GetLoadedSoundAssetPaths() {
    LogScope scope;
    std::vector<std::string> out;
    out.reserve(sSounds.size());
    for (const auto& kv : sSounds) out.push_back(kv.second.assetPath);
    std::sort(out.begin(), out.end());
    return out;
}

bool AudioManager::RenameSound(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    auto pathIt = sAssetPathToHandle.find(normalizedOld);
    if (pathIt == sAssetPathToHandle.end()) return false;
    const SoundHandle handle = pathIt->second;
    auto entryIt = sSounds.find(handle);
    if (entryIt == sSounds.end()) return false;

    SoundEntry &entry = entryIt->second;
    sAssetPathToHandle.erase(pathIt);
    sFileNameToHandle.erase(entry.fileName);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    entry.assetPath = normalizedNew;
    entry.fileName = PathToUtf8String(Utf8StringToPath(normalizedNew).filename());
    entry.fullPath = ProjectPaths::AssetsRoot() + "/" + normalizedNew;

    sAssetPathToHandle[normalizedNew] = handle;
    sFileNameToHandle[entry.fileName] = handle;
    return true;
}

AudioManager::PlayHandle AudioManager::Play(SoundHandle sound, float volume, float pitch, bool loop,
    double startTimeSec, double endTimeSec) {
    PlayParams params{};
    params.sound = sound;
    params.volume = volume;
    params.pitch = pitch;
    params.loop = loop;
    params.startTimeSec = startTimeSec;
    params.endTimeSec = endTimeSec;
    return Play(params);
}

AudioManager::PlayHandle AudioManager::Play(const PlayParams& params) {
    LogScope scope;
    if (params.sound == kInvalidSoundHandle) return kInvalidPlayHandle;

    if (!EnsureAudioInitialized()) return kInvalidPlayHandle;

    auto it = sSounds.find(params.sound);
    if (it == sSounds.end()) return kInvalidPlayHandle;

    const WAVEFORMATEX& wfex = it->second.wfex;
    if (wfex.nBlockAlign == 0 || wfex.nSamplesPerSec == 0) return kInvalidPlayHandle;

    const uint64_t totalFrames = it->second.buffer.size() / wfex.nBlockAlign;
    if (totalFrames == 0) return kInvalidPlayHandle;

    const double startSec = std::max(0.0, params.startTimeSec);
    const double endSec = params.endTimeSec;

    uint64_t startFrame = static_cast<uint64_t>(std::floor(startSec * static_cast<double>(wfex.nSamplesPerSec)));
    if (startFrame >= totalFrames) return kInvalidPlayHandle;

    bool hasEnd = endSec > 0.0;
    uint64_t endFrame = totalFrames;
    if (hasEnd) {
        const double clampedEnd = std::max(0.0, endSec);
        endFrame = static_cast<uint64_t>(std::floor(clampedEnd * static_cast<double>(wfex.nSamplesPerSec)));
        endFrame = std::min(endFrame, totalFrames);
        if (endFrame <= startFrame) return kInvalidPlayHandle;
    }

    const uint64_t playLengthFrames = hasEnd ? (endFrame - startFrame) : 0u;
    if (startFrame > std::numeric_limits<uint32_t>::max()) return kInvalidPlayHandle;
    if (hasEnd && playLengthFrames > std::numeric_limits<uint32_t>::max()) return kInvalidPlayHandle;

    const size_t idx = AcquirePlayIndex();
    if (idx == static_cast<size_t>(-1)) {
        Log(Translation("engine.audio.play.failed.toomany"), LogSeverity::Warning);
        return kInvalidPlayHandle;
    }

    if (idx >= sPlays.size() || !sPlays[idx]) return kInvalidPlayHandle;

    if (sUsedPlayIndices.find(idx) != sUsedPlayIndices.end()) {
        StopVoice(*sPlays[idx]);
    }

    PlayEntry& playEntry = *sPlays[idx];
    playEntry.sound = params.sound;
    playEntry.paused = false;
    playEntry.loop = params.loop;
    playEntry.startTimeSec = startSec;
    playEntry.sourceChannels = wfex.nChannels;
    playEntry.hasEffectChain = false;
    playEntry.eqEnabled = false;
    playEntry.echoEnabled = false;
    playEntry.limiterEnabled = false;

    // マスターボイスに加え、共有リバーブバスが使用可能ならそちらへも常時送る
    // (通常のウェットレベルは0で開始し、SetReverbSendで必要な時だけ送り量を上げる)
    XAUDIO2_SEND_DESCRIPTOR sendDescriptors[2] = {
        { 0, sMasterVoice },
        { 0, sReverbSubmixVoice },
    };
    const UINT32 sendCount = sReverbSubmixVoice ? 2u : 1u;
    XAUDIO2_VOICE_SENDS sendList{ sendCount, sendDescriptors };

    // EQ/エコー/リミッターのエフェクトチェーンを全ボイスへ無効状態で付与しておく
    // （SetEqEffect等の呼び出しで必要な時だけ有効化する。生成に失敗した場合はチェーン無しで続行する）
    Microsoft::WRL::ComPtr<IUnknown> eqEffect;
    Microsoft::WRL::ComPtr<IUnknown> echoEffect;
    Microsoft::WRL::ComPtr<IUnknown> limiterEffect;
    XAUDIO2_EFFECT_DESCRIPTOR effectDescriptors[3]{};
    XAUDIO2_EFFECT_CHAIN effectChain{ 3, effectDescriptors };
    XAUDIO2_EFFECT_CHAIN* effectChainPtr = nullptr;
    if (SUCCEEDED(CreateFX(__uuidof(FXEQ), &eqEffect)) &&
        SUCCEEDED(CreateFX(__uuidof(FXEcho), &echoEffect)) &&
        SUCCEEDED(CreateFX(__uuidof(FXMasteringLimiter), &limiterEffect))) {
        effectDescriptors[kEffectSlotEq] = { eqEffect.Get(), FALSE, wfex.nChannels };
        effectDescriptors[kEffectSlotEcho] = { echoEffect.Get(), FALSE, wfex.nChannels };
        effectDescriptors[kEffectSlotLimiter] = { limiterEffect.Get(), FALSE, wfex.nChannels };
        effectChainPtr = &effectChain;
    }

    HRESULT hr = sXaudio2->CreateSourceVoice(&playEntry.voice, &it->second.wfex, XAUDIO2_VOICE_USEFILTER,
        XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList, effectChainPtr);
    if (FAILED(hr) || !playEntry.voice) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createsourcevoice"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }
    playEntry.hasEffectChain = (effectChainPtr != nullptr);

    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(it->second.buffer.size());
    buffer.pAudioData = it->second.buffer.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.PlayBegin = static_cast<UINT32>(startFrame);
    if (hasEnd) {
        buffer.PlayLength = static_cast<UINT32>(playLengthFrames);
    }
    buffer.LoopCount = params.loop ? XAUDIO2_LOOP_INFINITE : 0;
    if (params.loop) {
        buffer.LoopBegin = buffer.PlayBegin;
        if (hasEnd) {
            buffer.LoopLength = buffer.PlayLength;
        }
    }

    hr = playEntry.voice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        StopVoice(playEntry);
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.submit"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    const float volume = std::clamp(params.volume, 0.0f, 1.0f);
    playEntry.voice->SetVolume(volume);
    playEntry.voice->SetFrequencyRatio(SemitonesToFrequencyRatio(params.pitch));

    if (sReverbSubmixVoice) {
        // リバーブバスへの送り量は既定で0(ドライのみ)にしておく。必要な時だけSetReverbSendで引き上げる
        std::vector<float> silentMatrix(static_cast<size_t>(playEntry.sourceChannels), 0.0f);
        playEntry.voice->SetOutputMatrix(sReverbSubmixVoice, playEntry.sourceChannels, 1, silentMatrix.data());
    }

    hr = playEntry.voice->Start();
    if (FAILED(hr)) {
        StopVoice(playEntry);
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.start"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    const PlayHandle playHandle = GenerateUniquePlayHandle();
    sUsedPlayIndices.insert(idx);
    sUsedPlayHandles.insert(playHandle);
    sPlayHandleToIndex[playHandle] = idx;

    return playHandle;
}

void AudioManager::RegisterSoundBeat(Passkey<SoundBeat>, SoundBeat* soundBeat) {
    if (!soundBeat) return;
    sRegisteredSoundBeats.insert(soundBeat);
}

void AudioManager::UnregisterSoundBeat(Passkey<SoundBeat>, SoundBeat* soundBeat) {
    if (!soundBeat) return;
    sRegisteredSoundBeats.erase(soundBeat);
}

void AudioManager::RegisterAudioPlayer(Passkey<AudioPlayer>, AudioPlayer* player) {
    if (!player) return;
    sRegisteredAudioPlayers.insert(player);
}

void AudioManager::UnregisterAudioPlayer(Passkey<AudioPlayer>, AudioPlayer* player) {
    if (!player) return;
    sRegisteredAudioPlayers.erase(player);
}

void AudioManager::Update() {
    LogScope scope;

    if (!sXaudio2) return;

    std::vector<PlayHandle> toStop;
    toStop.reserve(sPlayHandleToIndex.size());

    for (const auto& kv : sPlayHandleToIndex) {
        const PlayHandle playHandle = kv.first;
        const size_t idx = kv.second;

        if (idx >= sPlays.size() || !sPlays[idx]) continue;
        if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) continue;

        const PlayEntry& p = *sPlays[idx];
        if (!p.voice) continue;
        if (p.paused) continue;
        if (p.loop) continue;

        if (!IsVoiceActuallyPlaying(p.voice)) {
            toStop.push_back(playHandle);
        }
    }

    for (const auto h : toStop) {
        Stop(h);
    }

    for (auto* sb : sRegisteredSoundBeats) {
        sb->Update({});
    }

    for (auto* player : sRegisteredAudioPlayers) {
        if (player) player->Update({});
    }
}

bool AudioManager::Stop(PlayHandle play) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    StopVoice(*sPlays[idx]);
    ReleasePlayIndex(idx);
    ReleasePlayHandle(play);
    return true;
}

bool AudioManager::Pause(PlayHandle play) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice || p.paused) return false;
    p.voice->Stop();
    p.paused = true;
    return true;
}

bool AudioManager::Resume(PlayHandle play) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice || !p.paused) return false;
    p.voice->Start();
    p.paused = false;
    return true;
}

bool AudioManager::SetVolume(PlayHandle play, float volume) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;
    volume = std::clamp(volume, 0.0f, 1.0f);
    p.voice->SetVolume(volume);
    return true;
}

bool AudioManager::SetPitch(PlayHandle play, float pitch) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;
    const float ratio = SemitonesToFrequencyRatio(pitch);
    p.voice->SetFrequencyRatio(ratio);
    return true;
}

bool AudioManager::SetPan(PlayHandle play, float pan) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice || !sMasterVoice || p.sourceChannels == 0) return false;

    XAUDIO2_VOICE_DETAILS masterDetails{};
    sMasterVoice->GetVoiceDetails(&masterDetails);
    const uint32_t destChannels = masterDetails.InputChannels;
    const uint32_t srcChannels = p.sourceChannels;
    if (destChannels == 0) return false;

    std::vector<float> matrix(static_cast<size_t>(srcChannels) * destChannels, 0.0f);
    if (destChannels >= 2) {
        pan = std::clamp(pan, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979323846f;
        const float leftGain = std::cos(angle);
        const float rightGain = std::sin(angle);
        // SetOutputMatrixのpLevelMatrix仕様: 送出元チャンネルSから送出先チャンネルDへの係数は
        // pLevelMatrix[S + SourceChannels * D] に格納する(SourceChannelsが最も内側で変化する)
        for (uint32_t s = 0; s < srcChannels; ++s) {
            matrix[static_cast<size_t>(s) + static_cast<size_t>(srcChannels) * 0] = leftGain;
            matrix[static_cast<size_t>(s) + static_cast<size_t>(srcChannels) * 1] = rightGain;
        }
    } else {
        std::fill(matrix.begin(), matrix.end(), 1.0f);
    }

    const HRESULT hr = p.voice->SetOutputMatrix(sMasterVoice, srcChannels, destChannels, matrix.data());
    if (FAILED(hr)) {
        Log(Translation("engine.audio.play.failed.setoutputmatrix"), LogSeverity::Warning);
    }
    return SUCCEEDED(hr);
}

bool AudioManager::SetFilter(PlayHandle play, FilterType type, float frequency, float oneOverQ) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;

    XAUDIO2_FILTER_PARAMETERS params{};
    params.Type = ToXAudio2FilterType(type);
    params.Frequency = std::clamp(frequency, 0.0005f, XAUDIO2_MAX_FILTER_FREQUENCY);
    params.OneOverQ = std::clamp(oneOverQ, 0.0005f, XAUDIO2_MAX_FILTER_ONEOVERQ);
    const HRESULT hr = p.voice->SetFilterParameters(&params);
    return SUCCEEDED(hr);
}

bool AudioManager::SetReverbSend(PlayHandle play, float wetLevel) {
    LogScope scope;
    if (!sReverbSubmixVoice) return false;

    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice || p.sourceChannels == 0) return false;

    wetLevel = std::clamp(wetLevel, 0.0f, 1.0f);
    // リバーブ送り先(共有バス)の入力チャンネル数は1(モノ)固定
    const std::vector<float> matrix(static_cast<size_t>(p.sourceChannels), wetLevel);
    const HRESULT hr = p.voice->SetOutputMatrix(sReverbSubmixVoice, p.sourceChannels, 1, matrix.data());
    return SUCCEEDED(hr);
}

namespace {

/// @brief チェーンエフェクト付きの再生中PlayEntryを取得する（無効なハンドル・チェーン無しの場合はnullptr）
PlayEntry* GetPlayEntryWithEffectChain(PlayHandle play) {
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return nullptr;
    if (idx >= sPlays.size() || !sPlays[idx]) return nullptr;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return nullptr;

    PlayEntry& p = *sPlays[idx];
    if (!p.voice || !p.hasEffectChain) return nullptr;
    return &p;
}

/// @brief チェーンエフェクトの有効状態を必要な時だけ切り替える
/// @return 最終的にエフェクトが有効な場合 true
bool ApplyEffectEnabledState(PlayEntry& p, UINT32 slot, bool& cachedEnabled, bool enabled) {
    if (cachedEnabled == enabled) return enabled;
    if (enabled) {
        p.voice->EnableEffect(slot);
    } else {
        p.voice->DisableEffect(slot);
    }
    cachedEnabled = enabled;
    return enabled;
}

} // namespace

bool AudioManager::SetEqEffect(PlayHandle play, bool enabled, const EqParams &params) {
    LogScope scope;
    PlayEntry* p = GetPlayEntryWithEffectChain(play);
    if (!p) return false;

    if (!ApplyEffectEnabledState(*p, kEffectSlotEq, p->eqEnabled, enabled)) return true;

    FXEQ_PARAMETERS eq{};
    eq.FrequencyCenter0 = std::clamp(params.frequencyCenter[0], FXEQ_MIN_FREQUENCY_CENTER, FXEQ_MAX_FREQUENCY_CENTER);
    eq.FrequencyCenter1 = std::clamp(params.frequencyCenter[1], FXEQ_MIN_FREQUENCY_CENTER, FXEQ_MAX_FREQUENCY_CENTER);
    eq.FrequencyCenter2 = std::clamp(params.frequencyCenter[2], FXEQ_MIN_FREQUENCY_CENTER, FXEQ_MAX_FREQUENCY_CENTER);
    eq.FrequencyCenter3 = std::clamp(params.frequencyCenter[3], FXEQ_MIN_FREQUENCY_CENTER, FXEQ_MAX_FREQUENCY_CENTER);
    eq.Gain0 = std::clamp(params.gain[0], FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
    eq.Gain1 = std::clamp(params.gain[1], FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
    eq.Gain2 = std::clamp(params.gain[2], FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
    eq.Gain3 = std::clamp(params.gain[3], FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
    eq.Bandwidth0 = std::clamp(params.bandwidth[0], FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
    eq.Bandwidth1 = std::clamp(params.bandwidth[1], FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
    eq.Bandwidth2 = std::clamp(params.bandwidth[2], FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
    eq.Bandwidth3 = std::clamp(params.bandwidth[3], FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
    return SUCCEEDED(p->voice->SetEffectParameters(kEffectSlotEq, &eq, sizeof(eq)));
}

bool AudioManager::SetEchoEffect(PlayHandle play, bool enabled, const EchoParams &params) {
    LogScope scope;
    PlayEntry* p = GetPlayEntryWithEffectChain(play);
    if (!p) return false;

    if (!ApplyEffectEnabledState(*p, kEffectSlotEcho, p->echoEnabled, enabled)) return true;

    FXECHO_PARAMETERS echo{};
    echo.WetDryMix = std::clamp(params.wetDryMix, FXECHO_MIN_WETDRYMIX, FXECHO_MAX_WETDRYMIX);
    echo.Feedback = std::clamp(params.feedback, FXECHO_MIN_FEEDBACK, FXECHO_MAX_FEEDBACK);
    echo.Delay = std::clamp(params.delayMs, FXECHO_MIN_DELAY, FXECHO_MAX_DELAY);
    return SUCCEEDED(p->voice->SetEffectParameters(kEffectSlotEcho, &echo, sizeof(echo)));
}

bool AudioManager::SetLimiterEffect(PlayHandle play, bool enabled, const LimiterParams &params) {
    LogScope scope;
    PlayEntry* p = GetPlayEntryWithEffectChain(play);
    if (!p) return false;

    if (!ApplyEffectEnabledState(*p, kEffectSlotLimiter, p->limiterEnabled, enabled)) return true;

    FXMASTERINGLIMITER_PARAMETERS limiter{};
    limiter.Release = std::clamp(params.release,
        static_cast<uint32_t>(FXMASTERINGLIMITER_MIN_RELEASE), static_cast<uint32_t>(FXMASTERINGLIMITER_MAX_RELEASE));
    limiter.Loudness = std::clamp(params.loudness,
        static_cast<uint32_t>(FXMASTERINGLIMITER_MIN_LOUDNESS), static_cast<uint32_t>(FXMASTERINGLIMITER_MAX_LOUDNESS));
    return SUCCEEDED(p->voice->SetEffectParameters(kEffectSlotLimiter, &limiter, sizeof(limiter)));
}

bool AudioManager::IsPlaying(PlayHandle play) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    const PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;
    if (p.paused) return false;
    return IsVoiceActuallyPlaying(p.voice);
}

bool AudioManager::IsPaused(PlayHandle play) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    const PlayEntry& p = *sPlays[idx];
    if (!p.voice) return false;
    return p.paused;
}

#if defined(USE_IMGUI)
std::vector<AudioManager::SoundListEntry> AudioManager::GetImGuiSoundListEntries() {
    LogScope scope;

    std::vector<SoundListEntry> out;
    out.reserve(sSounds.size());

    for (const auto& kv : sSounds) {
        const auto& s = kv.second;
        SoundListEntry e;
        e.handle = kv.first;
        e.fileName = s.fileName;
        e.assetPath = s.assetPath;
        e.channels = static_cast<uint32_t>(s.wfex.nChannels);
        e.samplesPerSec = static_cast<uint32_t>(s.wfex.nSamplesPerSec);
        e.bitsPerSample = static_cast<uint32_t>(s.wfex.wBitsPerSample);
        e.durationMs = EstimateDurationMs(s.wfex, s.buffer);
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const SoundListEntry& a, const SoundListEntry& b) {
        return a.assetPath < b.assetPath;
    });

    return out;
}

std::vector<AudioManager::PlayingListEntry> AudioManager::GetImGuiPlayingListEntries() {
    LogScope scope;

    std::vector<PlayingListEntry> out;
    out.reserve(sPlayHandleToIndex.size());

    for (const auto& kv : sPlayHandleToIndex) {
        const PlayHandle playHandle = kv.first;
        const size_t idx = kv.second;

        if (idx >= sPlays.size() || !sPlays[idx]) continue;
        if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) continue;

        const PlayEntry& p = *sPlays[idx];
        if (p.sound == kInvalidSoundHandle) continue;

        PlayingListEntry e;
        e.playHandle = playHandle;
        e.soundHandle = p.sound;
        e.isPaused = p.paused;
        e.isPlaying = (!p.paused) && IsVoiceActuallyPlaying(p.voice);

        auto itS = sSounds.find(p.sound);
        if (itS != sSounds.end()) {
            e.fileName = itS->second.fileName;
            e.assetPath = itS->second.assetPath;
        }

        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const PlayingListEntry& a, const PlayingListEntry& b) {
        if (a.assetPath != b.assetPath) return a.assetPath < b.assetPath;
        return a.playHandle < b.playHandle;
    });

    return out;
}

void AudioManager::ShowImGuiLoadedSoundsWindow() {
    ImGui::Begin(TranslationLabel("editor.audiomanager.loaded.window"));

    const auto entries = GetImGuiSoundListEntries();
    ImGui::Text(TranslationC("editor.audiomanager.loaded_sounds_d"), static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    static float sVolume = 1.0f;
    static float sPitch = 0.0f;
    static bool sLoop = false;

    ImGui::Separator();
    ImGui::SliderFloat(TranslationLabel("editor.audiomanager.volume"), &sVolume, 0.0f, 1.0f);
    ImGui::SliderFloat(TranslationLabel("editor.audiomanager.pitch_semitones"), &sPitch, -24.0f, 24.0f);
    ImGui::Checkbox(TranslationLabel("editor.audiomanager.loop"), &sLoop);
    ImGui::Separator();

    if (ImGui::BeginTable("##SoundList", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("FileName");
        ImGui::TableSetupColumn("AssetPath");
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("PlayHandle", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableHeadersRow();

        for (const auto& e : entries) {
            if (filter.IsActive()) {
                if (!filter.PassFilter(e.fileName.c_str()) && !filter.PassFilter(e.assetPath.c_str())) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", e.handle);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.fileName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.assetPath.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text(TranslationC("editor.audiomanager.uch_uhz_ubit"), e.channels, e.samplesPerSec, e.bitsPerSample);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text(TranslationC("editor.audiomanager.ums"), e.durationMs);

            ImGui::TableSetColumnIndex(5);
            ImGui::PushID(static_cast<int>(e.handle));
            static PlayHandle sLastPlayHandle = kInvalidPlayHandle;
            if (ImGui::Button(TranslationLabel("editor.audiomanager.play"))) {
                sLastPlayHandle = Play(e.handle, sVolume, sPitch, sLoop);
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%u", sLastPlayHandle);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void AudioManager::ShowImGuiPlayingSoundsWindow() {
    ImGui::Begin(TranslationLabel("editor.audiomanager.playing.window"));

    const auto entries = GetImGuiPlayingListEntries();
    ImGui::Text(TranslationC("editor.audiomanager.active_plays_d"), static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    ImGui::Separator();

    if (ImGui::BeginTable("##PlayingList", 8,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
        ImGui::TableSetupColumn("PlayHandle", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("SoundHandle", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("FileName");
        ImGui::TableSetupColumn("AssetPath");
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Pause", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Resume", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (const auto& e : entries) {
            if (filter.IsActive()) {
                if (!filter.PassFilter(e.fileName.c_str()) && !filter.PassFilter(e.assetPath.c_str())) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", e.playHandle);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", e.soundHandle);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.fileName.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(e.assetPath.c_str());

            ImGui::TableSetColumnIndex(4);
            if (e.isPaused) {
                ImGui::TextUnformatted(TranslationC("editor.audiomanager.paused"));
            } else if (e.isPlaying) {
                ImGui::TextUnformatted(TranslationC("editor.audiomanager.playing"));
            } else {
                ImGui::TextUnformatted(TranslationC("editor.audiomanager.ended"));
            }

            ImGui::PushID(static_cast<int>(e.playHandle));

            ImGui::TableSetColumnIndex(5);
            if (ImGui::Button(TranslationLabel("editor.audiomanager.stop"))) {
                Stop(e.playHandle);
            }

            ImGui::TableSetColumnIndex(6);
            if (ImGui::Button(TranslationLabel("editor.audiomanager.pause"))) {
                Pause(e.playHandle);
            }

            ImGui::TableSetColumnIndex(7);
            if (ImGui::Button(TranslationLabel("editor.audiomanager.resume"))) {
                Resume(e.playHandle);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
