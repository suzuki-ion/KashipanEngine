#include "AudioManager.h"
#include "Assets/CaseInsensitive.h"
#include "Assets/AudioPlayer.h"
#include "Assets/SoundBeat.h"

#include "Debug/Logger.h"
#include "Utilities/Translation.h"
#include "Utilities/FileIO/Directory.h"

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
    IXAudio2SubmixVoice* effectVoice = nullptr;
    Microsoft::WRL::ComPtr<IUnknown> reverbEffect;
    Microsoft::WRL::ComPtr<IUnknown> eqEffect;
    Microsoft::WRL::ComPtr<IUnknown> echoEffect;
    XAUDIO2FX_REVERB_PARAMETERS reverbParams;
    FXEQ_PARAMETERS eqParams;
    FXECHO_PARAMETERS echoParams;
    bool reverbEnabled = false;
    bool eqEnabled = false;
    bool echoEnabled = false;
    bool paused = false;
    bool loop = false;
    double startTimeSec = 0.0;
};

std::unordered_map<SoundHandle, SoundEntry> sSounds;
FileMap<SoundHandle> sAssetPathToHandle;
FileMap<SoundHandle> sFileNameToHandle;

Microsoft::WRL::ComPtr<IXAudio2> sXaudio2;
IXAudio2MasteringVoice* sMasterVoice = nullptr;
bool sMfStarted = false;

static constexpr size_t kMaxSimultaneousPlays = 64;
std::vector<std::unique_ptr<PlayEntry>> sPlays;
std::unordered_set<size_t> sUsedPlayIndices;
std::vector<size_t> sFreePlayIndices;

std::unordered_map<PlayHandle, size_t> sPlayHandleToIndex;
std::unordered_set<PlayHandle> sUsedPlayHandles;

// Currently selected play handle for ImGui effects window
PlayHandle sSelectedPlayHandle = AudioManager::kInvalidPlayHandle;

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
    std::filesystem::path root(assetsRoot);
    std::filesystem::path full(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(full.filename().string());
    }
    return NormalizePathSlashes(rel.string());
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
    if (p.effectVoice) {
        p.effectVoice->DestroyVoice();
        p.effectVoice = nullptr;
    }
    p.reverbEffect.Reset();
    p.eqEffect.Reset();
    p.echoEffect.Reset();
    p.reverbParams;
    p.eqParams;
    p.echoParams;
    p.reverbEnabled = false;
    p.eqEnabled = false;
    p.echoEnabled = false;
    p.sound = AudioManager::kInvalidSoundHandle;
    p.paused = false;
    p.startTimeSec = 0.0;
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
    InitializeAudioDevice();
    LoadAllFromAssetsFolder();
}

AudioManager::~AudioManager() {
    LogScope scope;

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

    for (const auto& f : files) {
        Load(f);
    }
}

SoundHandle AudioManager::Load(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidSoundHandle;

    if (!EnsureAudioInitialized()) return kInvalidSoundHandle;

    const std::filesystem::path p(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.audio.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidSoundHandle;
    }

    if (!HasSupportedAudioExtension(p)) {
        Log(Translation("engine.audio.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
        return kInvalidSoundHandle;
    }

    const std::string full = NormalizePathSlashes(p.string());
    const std::string asset = MakeAssetRelativePath(assetsRootPath_, full);

    {
        auto it = sAssetPathToHandle.find(NormalizePathSlashes(asset));
        if (it != sAssetPathToHandle.end()) return it->second;
    }

    SoundEntry entry{};
    entry.fullPath = full;
    entry.assetPath = asset;
    entry.fileName = p.filename().string();

    const std::wstring wpath(p.wstring());
    if (!DecodeToPcm(wpath, entry.wfex, entry.buffer)) {
        Log(Translation("engine.audio.loading.failed.decode") + p.string(), LogSeverity::Warning);
        return kInvalidSoundHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidSoundHandle) {
        Log(Translation("engine.audio.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidSoundHandle;
    }

    Log(Translation("engine.audio.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

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
    playEntry.reverbParams;
    playEntry.eqParams;
    playEntry.echoParams;
    playEntry.reverbEnabled = false;
    playEntry.eqEnabled = false;
    playEntry.echoEnabled = false;

    Microsoft::WRL::ComPtr<IUnknown> reverb;
    HRESULT hr = XAudio2CreateReverb(&reverb, 0);
    if (FAILED(hr) || !reverb) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createeffect"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    Microsoft::WRL::ComPtr<IUnknown> eq;
    Microsoft::WRL::ComPtr<IUnknown> echo;
    hr = CreateFX(__uuidof(FXEQ), &eq, 0);
    if (FAILED(hr) || !eq) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createeffect"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    hr = CreateFX(__uuidof(FXEcho), &echo, 0);
    if (FAILED(hr) || !echo) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createeffect"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    IXAudio2SubmixVoice* effectVoice = nullptr;
    hr = sXaudio2->CreateSubmixVoice(&effectVoice, it->second.wfex.nChannels, it->second.wfex.nSamplesPerSec, 0, 0, nullptr, nullptr);
    if (FAILED(hr) || !effectVoice) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createsubmix"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    XAUDIO2_EFFECT_DESCRIPTOR effectDescs[3]{};
    effectDescs[0].pEffect = reverb.Get();
    effectDescs[0].InitialState = FALSE;
    effectDescs[0].OutputChannels = it->second.wfex.nChannels;

    effectDescs[1].pEffect = eq.Get();
    effectDescs[1].InitialState = FALSE;
    effectDescs[1].OutputChannels = it->second.wfex.nChannels;

    effectDescs[2].pEffect = echo.Get();
    effectDescs[2].InitialState = FALSE;
    effectDescs[2].OutputChannels = it->second.wfex.nChannels;

    XAUDIO2_EFFECT_CHAIN effectChain{};
    effectChain.EffectCount = 3;
    effectChain.pEffectDescriptors = effectDescs;

    hr = effectVoice->SetEffectChain(&effectChain);
    if (FAILED(hr)) {
        effectVoice->DestroyVoice();
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.seteffectchain"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    effectVoice->SetEffectParameters(0, &playEntry.reverbParams, sizeof(playEntry.reverbParams));
    effectVoice->SetEffectParameters(1, &playEntry.eqParams, sizeof(playEntry.eqParams));
    effectVoice->SetEffectParameters(2, &playEntry.echoParams, sizeof(playEntry.echoParams));

    XAUDIO2_SEND_DESCRIPTOR sendDesc{};
    sendDesc.Flags = 0;
    sendDesc.pOutputVoice = effectVoice;

    XAUDIO2_VOICE_SENDS sends{};
    sends.SendCount = 1;
    sends.pSends = &sendDesc;

    HRESULT hrSource = sXaudio2->CreateSourceVoice(&playEntry.voice, &it->second.wfex, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends, nullptr);
    hr = hrSource;
    if (FAILED(hr) || !playEntry.voice) {
        effectVoice->DestroyVoice();
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createsourcevoice"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    playEntry.effectVoice = effectVoice;
    playEntry.reverbEffect = reverb;
    playEntry.eqEffect = eq;
    playEntry.echoEffect = echo;

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

bool AudioManager::SetReverbParameters(PlayHandle play, const XAUDIO2FX_REVERB_PARAMETERS& params) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = p.effectVoice->SetEffectParameters(0, &params, sizeof(params));
    if (SUCCEEDED(hr)) {
        p.reverbParams = params;
    }
    return SUCCEEDED(hr);
}

bool AudioManager::EnableReverb(PlayHandle play, bool enable) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = enable ? p.effectVoice->EnableEffect(0, XAUDIO2_COMMIT_NOW)
                        : p.effectVoice->DisableEffect(0, XAUDIO2_COMMIT_NOW);
    if (SUCCEEDED(hr)) {
        p.reverbEnabled = enable;
    }
    return SUCCEEDED(hr);
}

bool AudioManager::SetEqParameters(PlayHandle play, const FXEQ_PARAMETERS& params) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = p.effectVoice->SetEffectParameters(1, &params, sizeof(params));
    if (SUCCEEDED(hr)) {
        p.eqParams = params;
    }
    return SUCCEEDED(hr);
}

bool AudioManager::EnableEq(PlayHandle play, bool enable) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = enable ? p.effectVoice->EnableEffect(1, XAUDIO2_COMMIT_NOW)
                        : p.effectVoice->DisableEffect(1, XAUDIO2_COMMIT_NOW);
    if (SUCCEEDED(hr)) {
        p.eqEnabled = enable;
    }
    return SUCCEEDED(hr);
}

bool AudioManager::SetEchoParameters(PlayHandle play, const FXECHO_PARAMETERS& params) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = p.effectVoice->SetEffectParameters(2, &params, sizeof(params));
    if (SUCCEEDED(hr)) {
        p.echoParams = params;
    }
    return SUCCEEDED(hr);
}

bool AudioManager::EnableEcho(PlayHandle play, bool enable) {
    LogScope scope;
    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(play, idx)) return false;
    if (idx >= sPlays.size() || !sPlays[idx]) return false;
    if (sUsedPlayIndices.find(idx) == sUsedPlayIndices.end()) return false;

    PlayEntry& p = *sPlays[idx];
    if (!p.effectVoice) return false;

    HRESULT hr = enable ? p.effectVoice->EnableEffect(2, XAUDIO2_COMMIT_NOW)
                        : p.effectVoice->DisableEffect(2, XAUDIO2_COMMIT_NOW);
    if (SUCCEEDED(hr)) {
        p.echoEnabled = enable;
    }
    return SUCCEEDED(hr);
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
    ImGui::Begin("AudioManager - Loaded Sounds");

    const auto entries = GetImGuiSoundListEntries();
    ImGui::Text("Loaded Sounds: %d", static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    static float sVolume = 1.0f;
    static float sPitch = 0.0f;
    static bool sLoop = false;

    ImGui::Separator();
    ImGui::SliderFloat("Volume", &sVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Pitch(semitones)", &sPitch, -24.0f, 24.0f);
    ImGui::Checkbox("Loop", &sLoop);
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
            ImGui::Text("%uch %uHz %ubit", e.channels, e.samplesPerSec, e.bitsPerSample);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%ums", e.durationMs);

            ImGui::TableSetColumnIndex(5);
            ImGui::PushID(static_cast<int>(e.handle));
            static PlayHandle sLastPlayHandle = kInvalidPlayHandle;
            if (ImGui::Button("Play")) {
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
    ImGui::Begin("AudioManager - Playing Sounds");

    const auto entries = GetImGuiPlayingListEntries();
    ImGui::Text("Active Plays: %d", static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    ImGui::Separator();

    if (ImGui::BeginTable("##PlayingList", 9,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 70);
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
            ImGui::PushID(static_cast<int>(e.playHandle));
            if (ImGui::Button("Select")) {
                sSelectedPlayHandle = e.playHandle;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", e.playHandle);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", e.soundHandle);

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(e.fileName.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(e.assetPath.c_str());

            ImGui::TableSetColumnIndex(5);
            if (e.isPaused) {
                ImGui::TextUnformatted("Paused");
            } else if (e.isPlaying) {
                ImGui::TextUnformatted("Playing");
            } else {
                ImGui::TextUnformatted("Ended");
            }

            ImGui::PushID(static_cast<int>(e.playHandle));

            ImGui::TableSetColumnIndex(6);
            if (ImGui::Button("Stop")) {
                Stop(e.playHandle);
            }

            ImGui::TableSetColumnIndex(7);
            if (ImGui::Button("Pause")) {
                Pause(e.playHandle);
            }

            ImGui::TableSetColumnIndex(8);
            if (ImGui::Button("Resume")) {
                Resume(e.playHandle);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
#endif

#if defined(USE_IMGUI)
void AudioManager::ShowImGuiEffectWindow() {
    ImGui::Begin("AudioManager - Effects");

    if (sSelectedPlayHandle == kInvalidPlayHandle) {
        ImGui::TextUnformatted("No play selected. Select a play in 'Playing Sounds' window.");
        ImGui::End();
        return;
    }

    size_t idx = static_cast<size_t>(-1);
    if (!TryGetPlayIndex(sSelectedPlayHandle, idx) || idx >= sPlays.size() || !sPlays[idx]) {
        ImGui::TextUnformatted("Selected play is not available.");
        if (ImGui::Button("Clear Selection")) {
            sSelectedPlayHandle = kInvalidPlayHandle;
        }
        ImGui::End();
        return;
    }

    PlayEntry& p = *sPlays[idx];

    ImGui::Text("Play: %u  Sound: %u", sSelectedPlayHandle, p.sound);
    const char* items[] = { "Reverb", "EQ", "Echo" };
    static int currentEffect = 0;
    ImGui::Combo("Effect", &currentEffect, items, IM_ARRAYSIZE(items));

    // Enabled toggle
    if (currentEffect == 0) {
        bool enabled = p.reverbEnabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            EnableReverb(sSelectedPlayHandle, enabled);
        }
    } else if (currentEffect == 1) {
        bool enabled = p.eqEnabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            EnableEq(sSelectedPlayHandle, enabled);
        }
    } else if (currentEffect == 2) {
        bool enabled = p.echoEnabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            EnableEcho(sSelectedPlayHandle, enabled);
        }
    }

    ImGui::Separator();

    // Generic parameter editor: treat parameter structs as float arrays and allow editing first few floats.
    // Use typed persistent edit copies per-play so UI widgets map to real member names and persist across frames.
    struct ReverbEdit {
        float WetDryMix;
        int ReflectionsDelay;
        int ReverbDelay;
        int RearDelay;
        int PositionLeft;
        int PositionRight;
        int PositionMatrixLeft;
        int PositionMatrixRight;
        int EarlyDiffusion;
        int LateDiffusion;
        int LowEQGain;
        int LowEQCutoff;
        int HighEQGain;
        int HighEQCutoff;
        float RoomFilterFreq;
        float RoomFilterMain;
        float RoomFilterHF;
        float ReflectionsGain;
        float ReverbGain;
        float DecayTime;
        float Density;
        float RoomSize;
        bool DisableLateField;
    };

    struct EqEdit {
        float FrequencyCenter0; float Gain0; float Bandwidth0;
        float FrequencyCenter1; float Gain1; float Bandwidth1;
        float FrequencyCenter2; float Gain2; float Bandwidth2;
        float FrequencyCenter3; float Gain3; float Bandwidth3;
    };

    struct EchoEdit {
        float WetDryMix;
        float Feedback;
        float Delay;
    };

    static std::unordered_map<PlayHandle, ReverbEdit> sReverbEdits;
    static std::unordered_map<PlayHandle, EqEdit> sEqEdits;
    static std::unordered_map<PlayHandle, EchoEdit> sEchoEdits;

    if (currentEffect == 0) {
        // Ensure edit exists
        auto it = sReverbEdits.find(sSelectedPlayHandle);
        if (it == sReverbEdits.end()) {
            ReverbEdit ed{};
            ed.WetDryMix = p.reverbParams.WetDryMix;
            ed.ReflectionsDelay = static_cast<int>(p.reverbParams.ReflectionsDelay);
            ed.ReverbDelay = static_cast<int>(p.reverbParams.ReverbDelay);
            ed.RearDelay = static_cast<int>(p.reverbParams.RearDelay);
#if defined(XAUDIO2FX_REVERB_MIN_7POINT1_SIDE_DELAY)
            // SideDelay exists on newer SDKs
            // We'll ignore SideDelay here (not exposed)
#endif
            ed.PositionLeft = static_cast<int>(p.reverbParams.PositionLeft);
            ed.PositionRight = static_cast<int>(p.reverbParams.PositionRight);
            ed.PositionMatrixLeft = static_cast<int>(p.reverbParams.PositionMatrixLeft);
            ed.PositionMatrixRight = static_cast<int>(p.reverbParams.PositionMatrixRight);
            ed.EarlyDiffusion = static_cast<int>(p.reverbParams.EarlyDiffusion);
            ed.LateDiffusion = static_cast<int>(p.reverbParams.LateDiffusion);
            ed.LowEQGain = static_cast<int>(p.reverbParams.LowEQGain);
            ed.LowEQCutoff = static_cast<int>(p.reverbParams.LowEQCutoff);
            ed.HighEQGain = static_cast<int>(p.reverbParams.HighEQGain);
            ed.HighEQCutoff = static_cast<int>(p.reverbParams.HighEQCutoff);
            ed.RoomFilterFreq = p.reverbParams.RoomFilterFreq;
            ed.RoomFilterMain = p.reverbParams.RoomFilterMain;
            ed.RoomFilterHF = p.reverbParams.RoomFilterHF;
            ed.ReflectionsGain = p.reverbParams.ReflectionsGain;
            ed.ReverbGain = p.reverbParams.ReverbGain;
            ed.DecayTime = p.reverbParams.DecayTime;
            ed.Density = p.reverbParams.Density;
            ed.RoomSize = p.reverbParams.RoomSize;
            ed.DisableLateField = (p.reverbParams.DisableLateField != FALSE);
            it = sReverbEdits.emplace(sSelectedPlayHandle, std::move(ed)).first;
        }

        ReverbEdit& ed = it->second;

        ImGui::DragFloat("WetDryMix", &ed.WetDryMix, 0.1f, 0.0f, 100.0f);
        ImGui::DragInt("ReflectionsDelay (ms)", &ed.ReflectionsDelay, 1, 0, 300);
        ImGui::DragInt("ReverbDelay (ms)", &ed.ReverbDelay, 1, 0, 85);
        ImGui::DragInt("RearDelay (ms)", &ed.RearDelay, 1, 0, 20);
        ImGui::DragInt("PositionLeft", &ed.PositionLeft, 1, 0, 30);
        ImGui::DragInt("PositionRight", &ed.PositionRight, 1, 0, 30);
        ImGui::DragInt("PositionMatrixLeft", &ed.PositionMatrixLeft, 1, 0, 30);
        ImGui::DragInt("PositionMatrixRight", &ed.PositionMatrixRight, 1, 0, 30);
        ImGui::DragInt("EarlyDiffusion", &ed.EarlyDiffusion, 1, 0, 15);
        ImGui::DragInt("LateDiffusion", &ed.LateDiffusion, 1, 0, 15);
        ImGui::DragInt("LowEQGain", &ed.LowEQGain, 1, 0, 12);
        ImGui::DragInt("LowEQCutoff", &ed.LowEQCutoff, 1, 0, 9);
        ImGui::DragInt("HighEQGain", &ed.HighEQGain, 1, 0, 8);
        ImGui::DragInt("HighEQCutoff", &ed.HighEQCutoff, 1, 0, 14);
        ImGui::DragFloat("RoomFilterFreq", &ed.RoomFilterFreq, 1.0f, 20.0f, 20000.0f);
        ImGui::DragFloat("RoomFilterMain (dB)", &ed.RoomFilterMain, 0.1f, -100.0f, 0.0f);
        ImGui::DragFloat("RoomFilterHF (dB)", &ed.RoomFilterHF, 0.1f, -100.0f, 0.0f);
        ImGui::DragFloat("ReflectionsGain (dB)", &ed.ReflectionsGain, 0.1f, -100.0f, 20.0f);
        ImGui::DragFloat("ReverbGain (dB)", &ed.ReverbGain, 0.1f, -100.0f, 20.0f);
        ImGui::DragFloat("DecayTime (s)", &ed.DecayTime, 0.01f, 0.1f, 10.0f);
        ImGui::DragFloat("Density", &ed.Density, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("RoomSize (ft)", &ed.RoomSize, 0.1f, 1.0f, 100.0f);
        ImGui::Checkbox("DisableLateField", &ed.DisableLateField);

        if (ImGui::Button("Apply")) {
            XAUDIO2FX_REVERB_PARAMETERS newParams = p.reverbParams;
            newParams.WetDryMix = ed.WetDryMix;
            newParams.ReflectionsDelay = static_cast<UINT32>(std::clamp(ed.ReflectionsDelay, 0, 300));
            newParams.ReverbDelay = static_cast<BYTE>(std::clamp(ed.ReverbDelay, 0, 85));
            newParams.RearDelay = static_cast<BYTE>(std::clamp(ed.RearDelay, 0, 20));
            newParams.PositionLeft = static_cast<BYTE>(std::clamp(ed.PositionLeft, 0, 30));
            newParams.PositionRight = static_cast<BYTE>(std::clamp(ed.PositionRight, 0, 30));
            newParams.PositionMatrixLeft = static_cast<BYTE>(std::clamp(ed.PositionMatrixLeft, 0, 30));
            newParams.PositionMatrixRight = static_cast<BYTE>(std::clamp(ed.PositionMatrixRight, 0, 30));
            newParams.EarlyDiffusion = static_cast<BYTE>(std::clamp(ed.EarlyDiffusion, 0, 15));
            newParams.LateDiffusion = static_cast<BYTE>(std::clamp(ed.LateDiffusion, 0, 15));
            newParams.LowEQGain = static_cast<BYTE>(std::clamp(ed.LowEQGain, 0, 12));
            newParams.LowEQCutoff = static_cast<BYTE>(std::clamp(ed.LowEQCutoff, 0, 9));
            newParams.HighEQGain = static_cast<BYTE>(std::clamp(ed.HighEQGain, 0, 8));
            newParams.HighEQCutoff = static_cast<BYTE>(std::clamp(ed.HighEQCutoff, 0, 14));
            newParams.RoomFilterFreq = ed.RoomFilterFreq;
            newParams.RoomFilterMain = ed.RoomFilterMain;
            newParams.RoomFilterHF = ed.RoomFilterHF;
            newParams.ReflectionsGain = ed.ReflectionsGain;
            newParams.ReverbGain = ed.ReverbGain;
            newParams.DecayTime = ed.DecayTime;
            newParams.Density = ed.Density;
            newParams.RoomSize = ed.RoomSize;
            newParams.DisableLateField = ed.DisableLateField ? TRUE : FALSE;

            SetReverbParameters(sSelectedPlayHandle, newParams);
        }

    } else if (currentEffect == 1) {
        auto it = sEqEdits.find(sSelectedPlayHandle);
        if (it == sEqEdits.end()) {
            EqEdit ed{};
            ed.FrequencyCenter0 = p.eqParams.FrequencyCenter0;
            ed.Gain0 = p.eqParams.Gain0;
            ed.Bandwidth0 = p.eqParams.Bandwidth0;
            ed.FrequencyCenter1 = p.eqParams.FrequencyCenter1;
            ed.Gain1 = p.eqParams.Gain1;
            ed.Bandwidth1 = p.eqParams.Bandwidth1;
            ed.FrequencyCenter2 = p.eqParams.FrequencyCenter2;
            ed.Gain2 = p.eqParams.Gain2;
            ed.Bandwidth2 = p.eqParams.Bandwidth2;
            ed.FrequencyCenter3 = p.eqParams.FrequencyCenter3;
            ed.Gain3 = p.eqParams.Gain3;
            ed.Bandwidth3 = p.eqParams.Bandwidth3;
            it = sEqEdits.emplace(sSelectedPlayHandle, std::move(ed)).first;
        }

        EqEdit& ed = it->second;
        ImGui::DragFloat("FrequencyCenter0 (Hz)", &ed.FrequencyCenter0, 1.0f, 20.0f, 20000.0f);
        ImGui::DragFloat("Gain0", &ed.Gain0, 0.01f, FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
        ImGui::DragFloat("Bandwidth0", &ed.Bandwidth0, 0.01f, FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
        ImGui::Separator();
        ImGui::DragFloat("FrequencyCenter1 (Hz)", &ed.FrequencyCenter1, 1.0f, 20.0f, 20000.0f);
        ImGui::DragFloat("Gain1", &ed.Gain1, 0.01f, FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
        ImGui::DragFloat("Bandwidth1", &ed.Bandwidth1, 0.01f, FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
        ImGui::Separator();
        ImGui::DragFloat("FrequencyCenter2 (Hz)", &ed.FrequencyCenter2, 1.0f, 20.0f, 20000.0f);
        ImGui::DragFloat("Gain2", &ed.Gain2, 0.01f, FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
        ImGui::DragFloat("Bandwidth2", &ed.Bandwidth2, 0.01f, FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);
        ImGui::Separator();
        ImGui::DragFloat("FrequencyCenter3 (Hz)", &ed.FrequencyCenter3, 1.0f, 20.0f, 20000.0f);
        ImGui::DragFloat("Gain3", &ed.Gain3, 0.01f, FXEQ_MIN_GAIN, FXEQ_MAX_GAIN);
        ImGui::DragFloat("Bandwidth3", &ed.Bandwidth3, 0.01f, FXEQ_MIN_BANDWIDTH, FXEQ_MAX_BANDWIDTH);

        if (ImGui::Button("Apply")) {
            FXEQ_PARAMETERS newParams;
            newParams.FrequencyCenter0 = ed.FrequencyCenter0; newParams.Gain0 = ed.Gain0; newParams.Bandwidth0 = ed.Bandwidth0;
            newParams.FrequencyCenter1 = ed.FrequencyCenter1; newParams.Gain1 = ed.Gain1; newParams.Bandwidth1 = ed.Bandwidth1;
            newParams.FrequencyCenter2 = ed.FrequencyCenter2; newParams.Gain2 = ed.Gain2; newParams.Bandwidth2 = ed.Bandwidth2;
            newParams.FrequencyCenter3 = ed.FrequencyCenter3; newParams.Gain3 = ed.Gain3; newParams.Bandwidth3 = ed.Bandwidth3;
            SetEqParameters(sSelectedPlayHandle, newParams);
        }

    } else if (currentEffect == 2) {
        auto it = sEchoEdits.find(sSelectedPlayHandle);
        if (it == sEchoEdits.end()) {
            EchoEdit ed{};
            ed.WetDryMix = p.echoParams.WetDryMix;
            ed.Feedback = p.echoParams.Feedback;
            ed.Delay = p.echoParams.Delay;
            it = sEchoEdits.emplace(sSelectedPlayHandle, std::move(ed)).first;
        }

        EchoEdit& ed = it->second;
        ImGui::DragFloat("WetDryMix", &ed.WetDryMix, 0.01f, FXECHO_MIN_WETDRYMIX, FXECHO_MAX_WETDRYMIX);
        ImGui::DragFloat("Feedback", &ed.Feedback, 0.01f, FXECHO_MIN_FEEDBACK, FXECHO_MAX_FEEDBACK);
        ImGui::DragFloat("Delay (ms)", &ed.Delay, 1.0f, FXECHO_MIN_DELAY, FXECHO_MAX_DELAY);

        if (ImGui::Button("Apply")) {
            FXECHO_PARAMETERS newParams;
            newParams.WetDryMix = ed.WetDryMix;
            newParams.Feedback = ed.Feedback;
            newParams.Delay = ed.Delay;
            SetEchoParameters(sSelectedPlayHandle, newParams);
        }
    }

    if (ImGui::Button("Clear Selection")) {
        sSelectedPlayHandle = kInvalidPlayHandle;
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
