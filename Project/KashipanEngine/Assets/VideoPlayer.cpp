#include "VideoPlayer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>

#include "Assets/AudioManager.h"
#include "Assets/TextureManager.h"
#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Graphics/VideoTexture.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/Translation.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace KashipanEngine {

namespace {

struct DecodedFrame final {
    std::vector<std::uint8_t> nv12Data;
    double timestampSec = 0.0;
};

} // namespace

struct VideoPlayer::Impl final {
    DirectXCommon *directXCommon = nullptr;
    std::string fullPath;
    std::string registerNamePrefix;

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // NV12出力の行ストライド（バイト単位、パディング込み）。0の場合はタイトパッキング（ストライド=幅）とみなす
    std::uint32_t videoStride = 0;

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;

    std::unique_ptr<VideoTexture> videoTexture;
    TextureManager::TextureHandle rgbaHandle = TextureManager::kInvalidHandle;

    AudioManager::SoundHandle soundHandle = AudioManager::kInvalidSoundHandle;
    AudioManager::PlayHandle playHandle = AudioManager::kInvalidPlayHandle;
    double audioDurationSec = 0.0;

    std::thread decodeThread;
    std::atomic<bool> stopRequested{ false };
    std::atomic<bool> endOfStream{ false };
    bool loop = false;

    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::deque<DecodedFrame> frameQueue;
    static constexpr std::size_t kMaxQueuedFrames = 3;

    bool isPlaying = false;
    bool isPaused = false;

    // デバッグ用: Play呼び出しからの経過時間計測（初回表示までの遅延調査のため）
    std::chrono::steady_clock::time_point playStartTime{};
    bool loggedFirstUpload = false;
    bool loggedPositionUnavailable = false;

    ~Impl() {
        LogScope scope;
        StopDecodeThread();
    }

    void StopDecodeThread() {
        LogScope scope;
        stopRequested = true;
        queueCv.notify_all();
        if (decodeThread.joinable()) {
            decodeThread.join();
        }
    }

    /// @brief 動画ストリームのネイティブなフレームサイズを取得する
    bool ProbeFrameSize() {
        LogScope scope;
        Microsoft::WRL::ComPtr<IMFMediaType> nativeType;
        HRESULT hr = reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &nativeType);
        if (FAILED(hr) || !nativeType) return false;

        UINT32 w = 0, h = 0;
        hr = MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &w, &h);
        if (FAILED(hr) || w == 0 || h == 0) return false;

        width = w;
        height = h;
        return true;
    }

    /// @brief 動画ストリームの出力形式をNV12に設定する（YUV→RGB変換はシェーダー側で行う）
    bool SetVideoOutputToNV12() {
        LogScope scope;
        Microsoft::WRL::ComPtr<IMFMediaType> type;
        HRESULT hr = MFCreateMediaType(&type);
        if (FAILED(hr) || !type) return false;

        hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (FAILED(hr)) return false;
        hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        if (FAILED(hr)) return false;

        hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type.Get());
        if (FAILED(hr)) return false;

        // 実際に選択された出力形式の行ストライド（パディング込みのバイト幅）を取得しておく。
        // デコーダによってはwidthちょうどではなく16等の境界に切り上げたストライドで出力するため、
        // ここを幅決め打ちにするとフレームによっては行がずれて崩れる/取りこぼす原因になる
        Microsoft::WRL::ComPtr<IMFMediaType> currentType;
        if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &currentType)) && currentType) {
            UINT32 strideRaw = 0;
            if (SUCCEEDED(currentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideRaw))) {
                const INT32 stride = static_cast<INT32>(strideRaw);
                if (stride > 0) {
                    videoStride = static_cast<std::uint32_t>(stride);
                }
            }
        }
        return true;
    }

    /// @brief 音声ストリームをPCMへ設定し、全体をデコードしてAudioManagerへ登録する
    /// @details AudioManager::DecodeToPcm(Assets/AudioManager.cpp)と同じ手順で行う
    bool DecodeAudioTrackToPcm() {
        LogScope scope;
        Microsoft::WRL::ComPtr<IMFMediaType> type;
        HRESULT hr = MFCreateMediaType(&type);
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

        WAVEFORMATEX *wf = nullptr;
        hr = MFCreateWaveFormatExFromMFMediaType(type.Get(), &wf, nullptr);
        if (FAILED(hr) || !wf) return false;
        const WAVEFORMATEX wfex = *wf;
        CoTaskMemFree(wf);

        std::vector<std::uint8_t> buffer;
        while (true) {
            DWORD flags = 0;
            Microsoft::WRL::ComPtr<IMFSample> sample;
            hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample);
            if (FAILED(hr)) return false;
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
            if (!sample) continue;

            Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
            hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
            if (FAILED(hr) || !mediaBuffer) return false;

            BYTE *data = nullptr;
            DWORD curLen = 0;
            hr = mediaBuffer->Lock(&data, nullptr, &curLen);
            if (FAILED(hr) || !data || curLen == 0) {
                mediaBuffer->Unlock();
                return false;
            }

            const std::size_t oldSize = buffer.size();
            buffer.resize(oldSize + curLen);
            std::memcpy(buffer.data() + oldSize, data, curLen);
            mediaBuffer->Unlock();
        }
        if (buffer.empty()) return false;

        audioDurationSec = wfex.nAvgBytesPerSec > 0
            ? static_cast<double>(buffer.size()) / static_cast<double>(wfex.nAvgBytesPerSec)
            : 0.0;

        soundHandle = AudioManager::RegisterSoundFromMemory(registerNamePrefix + "_Audio",
            wfex.nChannels, wfex.nSamplesPerSec, wfex.wBitsPerSample, buffer);
        return soundHandle != AudioManager::kInvalidSoundHandle;
    }

    /// @brief 映像ストリームを専用スレッドでストリーミングデコードし、フレームキューへ積み続ける
    void DecodeThreadMain() {
        LogScope scope;
        // Media FoundationのAPIを呼ぶスレッドはCOMを初期化しておく必要がある
        const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool needCoUninit = SUCCEEDED(coHr);

        bool loggedFirstFrame = false;
        std::uint32_t decodedFrameCount = 0;

        while (!stopRequested) {
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            Microsoft::WRL::ComPtr<IMFSample> sample;
            HRESULT hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &timestamp, &sample);
            if (FAILED(hr)) {
                Log(Translation("engine.video.decode.failed.readsample") + registerNamePrefix, LogSeverity::Error);
                break;
            }

            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                if (loop) {
                    PROPVARIANT position{};
                    position.vt = VT_I8;
                    position.hVal.QuadPart = 0;
                    const HRESULT seekHr = reader->SetCurrentPosition(GUID_NULL, position);
                    if (FAILED(seekHr)) {
                        Log(Translation("engine.video.decode.failed.seek") + registerNamePrefix, LogSeverity::Error);
                        break;
                    }
                    // キューに前回ループ末尾の古い(タイムスタンプが大きい)フレームが残ったままだと、
                    // これから積む新しいフレーム(タイムスタンプが0付近)より手前に居座り続けてしまい、
                    // Update側の「先頭のタイムスタンプが現在位置以下になるまでポップする」ロジックが
                    // （音声側は既にループしてポジションが0付近に戻っているため）永久に進まなくなる。
                    // ループの区切りで明示的にキューをクリアする
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        frameQueue.clear();
                    }
                    queueCv.notify_all();
                    continue;
                }
                Log(Translation("engine.video.decode.endofstream") + registerNamePrefix, LogSeverity::Info);
                endOfStream = true;
                break;
            }
            if (!sample) continue;

            Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
            hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr) || !buffer) {
                Log(Translation("engine.video.decode.failed.buffer") + registerNamePrefix, LogSeverity::Error);
                continue;
            }

            BYTE *data = nullptr;
            DWORD dataLen = 0;
            hr = buffer->Lock(&data, nullptr, &dataLen);
            if (FAILED(hr) || !data) {
                Log(Translation("engine.video.decode.failed.lock") + registerNamePrefix, LogSeverity::Error);
                continue;
            }

            DecodedFrame frame;
            frame.nv12Data.assign(data, data + dataLen);
            frame.timestampSec = static_cast<double>(timestamp) / 10000000.0;
            buffer->Unlock();

            if (!loggedFirstFrame) {
                loggedFirstFrame = true;
                Log(Translation("engine.video.decode.firstframe") + registerNamePrefix +
                    " (" + std::to_string(dataLen) + " bytes, expected " +
                    std::to_string(static_cast<std::size_t>(videoStride != 0 ? videoStride : width) * height * 3 / 2) + ")",
                    LogSeverity::Info);
            }
            ++decodedFrameCount;

            std::unique_lock<std::mutex> lock(queueMutex);
            queueCv.wait(lock, [this] { return frameQueue.size() < kMaxQueuedFrames || stopRequested.load(); });
            if (stopRequested) break;
            frameQueue.push_back(std::move(frame));
        }

        if (decodedFrameCount == 0) {
            Log(Translation("engine.video.decode.zeroframes") + registerNamePrefix, LogSeverity::Warning);
        }

        if (needCoUninit) {
            CoUninitialize();
        }
    }
};

VideoPlayer::VideoPlayer(DirectXCommon *directXCommon, std::string fullPath, std::string registerNamePrefix)
    : impl_(std::make_unique<Impl>()), fullPath_(fullPath) {
    LogScope scope;
    impl_->directXCommon = directXCommon;
    impl_->fullPath = std::move(fullPath);
    impl_->registerNamePrefix = std::move(registerNamePrefix);
}

VideoPlayer::~VideoPlayer() {
    LogScope scope;
    Stop();
}

bool VideoPlayer::Play(bool loop, float volume) {
    LogScope scope;
    if (impl_->isPlaying) return false;
    if (impl_->fullPath.empty()) return false;

    // デバッグ用: 初回表示までの遅延調査のため、Play呼び出し時刻を記録しておく
    impl_->playStartTime = std::chrono::steady_clock::now();
    impl_->loggedFirstUpload = false;

    const std::filesystem::path p = Utf8StringToPath(impl_->fullPath);
    const std::wstring wpath(p.wstring());

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) {
        Log(Translation("engine.video.play.failed.reader") + impl_->fullPath, LogSeverity::Error);
        return false;
    }
    impl_->reader = reader;

    // いったん全ストリームを無効化し、必要なストリームだけを都度有効化しながら準備を進める
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

    if (!impl_->ProbeFrameSize() || !impl_->SetVideoOutputToNV12()) {
        Log(Translation("engine.video.play.failed.videoformat") + impl_->fullPath, LogSeverity::Error);
        impl_->reader.Reset();
        return false;
    }

    // 音声トラックを先に全体デコードする（映像ストリームは後で先頭からストリーミング開始するため無効化しておく）
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), FALSE);
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);

    if (!impl_->DecodeAudioTrackToPcm()) {
        Log(Translation("engine.video.play.failed.audiodecode") + impl_->fullPath, LogSeverity::Error);
        impl_->reader.Reset();
        return false;
    }

    // 音声を最後まで読み切ったため、映像デコードのために先頭へシークし直す
    // ※ シークは対象ストリーム（映像）を選択した状態で行わないと反映されないことがあるため、
    //    ストリーム選択の切り替えを先に行ってからシークする
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), FALSE);
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);
    {
        PROPVARIANT position{};
        position.vt = VT_I8;
        position.hVal.QuadPart = 0;
        const HRESULT seekHr = impl_->reader->SetCurrentPosition(GUID_NULL, position);
        if (FAILED(seekHr)) {
            Log(Translation("engine.video.play.failed.seek") + impl_->fullPath, LogSeverity::Warning);
        }
    }

    impl_->videoTexture = std::make_unique<VideoTexture>(impl_->directXCommon, impl_->width, impl_->height);
    if (!impl_->videoTexture->IsValid()) {
        Log(Translation("engine.video.play.failed.texture") + impl_->fullPath, LogSeverity::Error);
        impl_->reader.Reset();
        impl_->videoTexture.reset();
        return false;
    }

    impl_->rgbaHandle = TextureManager::RegisterExternalTexture(impl_->registerNamePrefix + "_RGBA", impl_->videoTexture->GetRgbaView());

    impl_->loop = loop;
    impl_->stopRequested = false;
    impl_->endOfStream = false;
    impl_->decodeThread = std::thread([this] { impl_->DecodeThreadMain(); });

    impl_->playHandle = AudioManager::Play(impl_->soundHandle, volume, 0.0f, loop);
    if (impl_->playHandle == AudioManager::kInvalidPlayHandle) {
        Log(Translation("engine.video.play.failed.audioplay") + impl_->fullPath, LogSeverity::Error);
        Stop();
        return false;
    }

    impl_->isPlaying = true;
    impl_->isPaused = false;
    Log(Translation("engine.video.play.succeeded") + impl_->fullPath, LogSeverity::Info);
    return true;
}

bool VideoPlayer::ShowFirstFrame() {
    LogScope scope;
    if (impl_->isPlaying) return false;
    if (impl_->fullPath.empty()) return false;
    // 既にこのインスタンスで（このメソッド経由で）1フレーム目を用意済みなら何もしない
    if (impl_->reader) return true;

    const std::filesystem::path p = Utf8StringToPath(impl_->fullPath);
    const std::wstring wpath(p.wstring());

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) {
        Log(Translation("engine.video.play.failed.reader") + impl_->fullPath, LogSeverity::Error);
        return false;
    }
    impl_->reader = reader;

    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

    if (!impl_->ProbeFrameSize() || !impl_->SetVideoOutputToNV12()) {
        Log(Translation("engine.video.play.failed.videoformat") + impl_->fullPath, LogSeverity::Error);
        impl_->reader.Reset();
        return false;
    }

    impl_->videoTexture = std::make_unique<VideoTexture>(impl_->directXCommon, impl_->width, impl_->height);
    if (!impl_->videoTexture->IsValid()) {
        Log(Translation("engine.video.play.failed.texture") + impl_->fullPath, LogSeverity::Error);
        impl_->reader.Reset();
        impl_->videoTexture.reset();
        return false;
    }
    impl_->rgbaHandle = TextureManager::RegisterExternalTexture(impl_->registerNamePrefix + "_RGBA", impl_->videoTexture->GetRgbaView());

    DWORD flags = 0;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = impl_->reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, nullptr, &sample);
    if (SUCCEEDED(hr) && sample) {
        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
            BYTE *data = nullptr;
            DWORD dataLen = 0;
            if (SUCCEEDED(buffer->Lock(&data, nullptr, &dataLen)) && data) {
                impl_->videoTexture->UploadFrame(data, dataLen, impl_->videoStride);
                buffer->Unlock();
            }
        }
    } else {
        Log(Translation("engine.video.play.failed.firstframe") + impl_->fullPath, LogSeverity::Warning);
    }

    // isPlaying/isPausedはfalseのまま維持する（このインスタンスは「再生中」扱いにしない）
    return true;
}

void VideoPlayer::Stop() {
    LogScope scope;
    if (!impl_->isPlaying && !impl_->reader) return;

    impl_->StopDecodeThread();

    if (impl_->playHandle != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(impl_->playHandle);
        impl_->playHandle = AudioManager::kInvalidPlayHandle;
    }
    if (impl_->rgbaHandle != TextureManager::kInvalidHandle) {
        TextureManager::UnregisterExternalTexture(impl_->rgbaHandle);
        impl_->rgbaHandle = TextureManager::kInvalidHandle;
    }

    impl_->videoTexture.reset();
    impl_->reader.Reset();
    {
        std::lock_guard<std::mutex> lock(impl_->queueMutex);
        impl_->frameQueue.clear();
    }

    impl_->isPlaying = false;
    impl_->isPaused = false;
}

void VideoPlayer::Pause() {
    LogScope scope;
    if (!impl_->isPlaying || impl_->isPaused) return;
    AudioManager::Pause(impl_->playHandle);
    impl_->isPaused = true;
}

void VideoPlayer::Resume() {
    LogScope scope;
    if (!impl_->isPlaying || !impl_->isPaused) return;
    AudioManager::Resume(impl_->playHandle);
    impl_->isPaused = false;
}

bool VideoPlayer::IsPlaying() const noexcept { LogScope scope; return impl_->isPlaying; }
bool VideoPlayer::IsPaused() const noexcept { LogScope scope; return impl_->isPaused; }

void VideoPlayer::Update() {
    LogScope scope;
    if (!impl_->isPlaying || impl_->isPaused) return;

    // デバッグ用: Play直後の数秒間、Updateが実際にどの頻度で呼ばれ、キューがどう変化しているかを
    // 逐一記録する（初回表示までの遅延調査のため）。表示済みなら早期に打ち切る
    if (!impl_->loggedFirstUpload) {
        const auto elapsedMsForTrace = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - impl_->playStartTime).count();
        if (elapsedMsForTrace < 6000) {
            std::size_t queueSizeForTrace = 0;
            double queueFrontTimestampForTrace = -1.0;
            {
                std::lock_guard<std::mutex> lock(impl_->queueMutex);
                queueSizeForTrace = impl_->frameQueue.size();
                if (!impl_->frameQueue.empty()) queueFrontTimestampForTrace = impl_->frameQueue.front().timestampSec;
            }
            Log("[VideoDebugTrace] " + impl_->registerNamePrefix +
                " +" + std::to_string(elapsedMsForTrace) + "ms queueSize=" + std::to_string(queueSizeForTrace) +
                " queueFront=" + std::to_string(queueFrontTimestampForTrace), LogSeverity::Info);
        }
    }

    double positionSec = 0.0;
    if (!AudioManager::GetPlayPositionSeconds(impl_->playHandle, positionSec)) {
        if (!impl_->loggedPositionUnavailable) {
            impl_->loggedPositionUnavailable = true;
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - impl_->playStartTime).count();
            Log(Translation("engine.video.debug.positionunavailable") + impl_->registerNamePrefix +
                " (+" + std::to_string(elapsedMs) + "ms)", LogSeverity::Warning);
        }
        return;
    }
    if (impl_->loop && impl_->audioDurationSec > 0.0) {
        positionSec = std::fmod(positionSec, impl_->audioDurationSec);
    }

    DecodedFrame frameToShow;
    bool hasFrame = false;
    {
        std::lock_guard<std::mutex> lock(impl_->queueMutex);
        // 表示すべき時刻を過ぎているフレームは（追いつくために複数あれば）読み捨てながら最新の1枚を採用する
        while (!impl_->frameQueue.empty() && impl_->frameQueue.front().timestampSec <= positionSec) {
            frameToShow = std::move(impl_->frameQueue.front());
            impl_->frameQueue.pop_front();
            hasFrame = true;
        }
        if (hasFrame) {
            impl_->queueCv.notify_all();
        }
    }

    if (hasFrame && impl_->videoTexture) {
        // UploadFrameがfalseを返すのは、前回のGPUコピーが未完了で今回はスキップした場合も含む
        // （次のUpdateで再試行される想定の正常系のため、ここではログを出さない。データサイズ不整合等の
        // 実際の異常はVideoTexture::UploadFrame内でログする）
        const bool uploaded = impl_->videoTexture->UploadFrame(
            frameToShow.nv12Data.data(), frameToShow.nv12Data.size(), impl_->videoStride);
        if (uploaded && !impl_->loggedFirstUpload) {
            impl_->loggedFirstUpload = true;
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - impl_->playStartTime).count();
            Log(Translation("engine.video.debug.firstupload") + impl_->registerNamePrefix +
                " (+" + std::to_string(elapsedMs) + "ms, positionSec=" + std::to_string(positionSec) +
                ", frameTimestamp=" + std::to_string(frameToShow.timestampSec) + ")", LogSeverity::Info);
        }
    }
}

void VideoPlayer::ConfigureMaterial(MaterialManager::MaterialHandle materialHandle) const {
    LogScope scope;
    auto *material = MaterialManager::GetMaterial(materialHandle);
    if (!material) return;

    // 変換後は通常のRGBAテクスチャなので、既存のtextureHandleを単純に上書きするだけでよい
    material->textureHandle = impl_->rgbaHandle;
}

VideoTexture *VideoPlayer::GetVideoTexture(Passkey<VideoManager>) const {
    LogScope scope;
    return impl_->videoTexture.get();
}

std::uint32_t VideoPlayer::GetWidth() const noexcept { LogScope scope; return impl_->width; }
std::uint32_t VideoPlayer::GetHeight() const noexcept { LogScope scope; return impl_->height; }

TextureManager::TextureHandle VideoPlayer::GetTextureHandle() const noexcept { LogScope scope; return impl_->rgbaHandle; }

AudioManager::PlayHandle VideoPlayer::GetAudioPlayHandle() const noexcept { LogScope scope; return impl_->playHandle; }

} // namespace KashipanEngine
