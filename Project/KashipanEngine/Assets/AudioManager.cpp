#include "AudioManager.h"

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
    bool paused = false;
    bool loop = false;
};

std::unordered_map<SoundHandle, SoundEntry> sSounds;
std::unordered_map<std::string, SoundHandle> sAssetPathToHandle;
std::unordered_map<std::string, SoundHandle> sFileNameToHandle;

Microsoft::WRL::ComPtr<IXAudio2> sXaudio2;
IXAudio2MasteringVoice* sMasterVoice = nullptr;
bool sMfStarted = false;

static constexpr size_t kMaxSimultaneousPlays = 64;
std::vector<std::unique_ptr<PlayEntry>> sPlays;
std::unordered_set<size_t> sUsedPlayIndices;
std::vector<size_t> sFreePlayIndices;

std::unordered_map<PlayHandle, size_t> sPlayHandleToIndex;
std::unordered_set<PlayHandle> sUsedPlayHandles;

static std::mt19937& Rng() {
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
    p.sound = AudioManager::kInvalidSoundHandle;
    p.paused = false;
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

AudioManager::PlayHandle AudioManager::Play(SoundHandle sound, float volume, float pitch, bool loop) {
    LogScope scope;
    if (sound == kInvalidSoundHandle) return kInvalidPlayHandle;

    if (!EnsureAudioInitialized()) return kInvalidPlayHandle;

    auto it = sSounds.find(sound);
    if (it == sSounds.end()) return kInvalidPlayHandle;

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
    playEntry.sound = sound;
    playEntry.paused = false;
    playEntry.loop = loop;

    HRESULT hr = sXaudio2->CreateSourceVoice(&playEntry.voice, &it->second.wfex);
    if (FAILED(hr) || !playEntry.voice) {
        playEntry.sound = kInvalidSoundHandle;
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.createsourcevoice"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(it->second.buffer.size());
    buffer.pAudioData = it->second.buffer.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    hr = playEntry.voice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        StopVoice(playEntry);
        ReleasePlayIndex(idx);
        Log(Translation("engine.audio.play.failed.submit"), LogSeverity::Error);
        return kInvalidPlayHandle;
    }

    volume = std::clamp(volume, 0.0f, 1.0f);
    playEntry.voice->SetVolume(volume);
    playEntry.voice->SetFrequencyRatio(SemitonesToFrequencyRatio(pitch));

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
                ImGui::TextUnformatted("Paused");
            } else if (e.isPlaying) {
                ImGui::TextUnformatted("Playing");
            } else {
                ImGui::TextUnformatted("Ended");
            }

            ImGui::PushID(static_cast<int>(e.playHandle));

            ImGui::TableSetColumnIndex(5);
            if (ImGui::Button("Stop")) {
                Stop(e.playHandle);
            }

            ImGui::TableSetColumnIndex(6);
            if (ImGui::Button("Pause")) {
                Pause(e.playHandle);
            }

            ImGui::TableSetColumnIndex(7);
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

} // namespace KashipanEngine
