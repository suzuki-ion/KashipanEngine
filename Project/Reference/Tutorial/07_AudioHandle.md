# 7. 音声のハンドルを取得し、再生や停止をしたいとき

音声は `AudioManager` が管理し、
- 音声リソース：`AudioManager::SoundHandle`（`uint32_t`）
- 再生インスタンス：`AudioManager::PlayHandle`（`uint32_t`）

で扱います。

- ヘッダ：`KashipanEngine/Assets/AudioManager.h`

---

## 7.1 ハンドル型と無効値

- `AudioManager::SoundHandle`：`uint32_t`
  - 無効値：`AudioManager::kInvalidSoundHandle`（0）
- `AudioManager::PlayHandle`：`uint32_t`
  - 無効値：`AudioManager::kInvalidPlayHandle`（0）

---

## 7.2 音声ハンドルの取得（既にロード済みの前提）

- ファイル名単体から取得：

```cpp
auto se = KashipanEngine::AudioManager::GetSoundHandleFromFileName("se_shot.wav");
```

- Assets ルート相対パスから取得：

```cpp
auto se = KashipanEngine::AudioManager::GetSoundHandleFromAssetPath("Sounds/se_shot.wav");
```

---

## 7.3 再生/停止（実API）

`Play(...)` の実シグネチャは以下です。

- `static PlayHandle Play(SoundHandle sound, float volume = 1.0f, float pitch = 0.0f, bool loop = false);`

```cpp
#include <KashipanEngine.h>

auto se = KashipanEngine::AudioManager::GetSoundHandleFromFileName("se_shot.wav");

// 再生（volume: 0..1, pitch: 半音単位, loop: ループ）
auto play = KashipanEngine::AudioManager::Play(se, 0.8f, 0.0f, false);

// 停止（成功時 true）
KashipanEngine::AudioManager::Stop(play);
```

---

## 7.4 一時停止/再開（成功時 true）

```cpp
auto play = KashipanEngine::AudioManager::Play(se);
KashipanEngine::AudioManager::Pause(play);
KashipanEngine::AudioManager::Resume(play);
```

---

## 7.5 再生中パラメータ

```cpp
KashipanEngine::AudioManager::SetVolume(play, 0.5f);
KashipanEngine::AudioManager::SetPitch(play, +1.0f); // +1.0f で半音上

bool playing = KashipanEngine::AudioManager::IsPlaying(play);
bool paused = KashipanEngine::AudioManager::IsPaused(play);
(void)playing;
(void)paused;
```

---

## 7.6 再生位置（秒）の取得

```cpp
double sec = 0.0;
if (KashipanEngine::AudioManager::GetPlayPositionSeconds(play, sec)) {
    // sec に秒数が入る
}
```

---

## 注意点（厳密）

- 取得/読み込み/再生に失敗した場合は無効ハンドル（0）が返ります。
- 事前ロード対象/対応拡張子/ストリーミング等の扱いは `AudioManager` 実装に従います。
