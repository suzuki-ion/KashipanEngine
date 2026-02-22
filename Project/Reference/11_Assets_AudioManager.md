# アセット管理：音声（`AudioManager`）

対象ヘッダ：`KashipanEngine/Assets/AudioManager.h`

---

## 1. 目的

音声ファイルの読み込み・再生・制御を行うクラスです。
XAudio2 と Media Foundation を使用して多様な音声形式を PCM にデコードし、再生します。

---

## 2. 設計概要

- 音声リソースは `SoundHandle`（`uint32_t`）で管理します。
- 再生インスタンスは `PlayHandle`（`uint32_t`）で管理します。
- 無効ハンドルはそれぞれ `kInvalidSoundHandle`（`0`）、`kInvalidPlayHandle`（`0`）です。
- `Assets/` 配下のファイルはエンジン起動時に `LoadAllFromAssetsFolder()` で自動ロードされます。
- ファイル名・アセットパスでの検索は大文字小文字を区別しません。
- 同時再生数は最大 64 です。

### 対応音声形式

`.wav` / `.mp3` / `.ogg` / `.flac` / `.aac` / `.m4a` / `.wma`

---

## 3. 公開API

### 3.1 型定義・定数

```cpp
using SoundHandle = uint32_t;
static constexpr SoundHandle kInvalidSoundHandle = 0;

using PlayHandle = uint32_t;
static constexpr PlayHandle kInvalidPlayHandle = 0;
```

### 3.2 `PlayParams`（再生パラメータ構造体）

```cpp
struct PlayParams final {
    SoundHandle sound = kInvalidSoundHandle;
    float volume = 1.0f;       // 0.0f ~ 1.0f
    float pitch = 0.0f;        // 半音単位（+1.0f で半音上がる）
    bool loop = false;
    double startTimeSec = 0.0; // 再生開始時間（秒）
    double endTimeSec = 0.0;   // 再生終了時間（秒。0以下は末尾まで）
};
```

### 3.3 コンストラクタ（`GameEngine` からのみ生成可能）

```cpp
AudioManager(Passkey<GameEngine>, const std::string& assetsRootPath = "Assets");
```

### 3.4 読み込み

```cpp
SoundHandle Load(const std::string& filePath);
```

- `Assets` ルート相対パス or フルパスで音声をロードします。
- Media Foundation を使用して PCM にデコードします。
- 既にロード済みのパスが指定された場合は既存のハンドルを返します。
- 失敗時は `kInvalidSoundHandle` を返します。

### 3.5 ハンドル取得

```cpp
static SoundHandle GetSoundHandleFromFileName(const std::string& fileName);
static SoundHandle GetSoundHandleFromAssetPath(const std::string& assetPath);
```

### 3.6 再生

```cpp
static PlayHandle Play(SoundHandle sound, float volume = 1.0f, float pitch = 0.0f,
    bool loop = false, double startTimeSec = 0.0, double endTimeSec = 0.0);
static PlayHandle Play(const PlayParams& params);
```

- 指定した音声を再生し、再生ハンドルを返します。
- `pitch` は半音単位で指定します（`+1.0f` で半音上がる、`-12.0f` で1オクターブ下がる）。
- `startTimeSec` / `endTimeSec` で再生範囲を指定できます。`endTimeSec` が `0` 以下の場合は末尾まで再生します。
- 失敗時は `kInvalidPlayHandle` を返します。

### 3.7 停止・一時停止・再開

```cpp
static bool Stop(PlayHandle play);
static bool Pause(PlayHandle play);
static bool Resume(PlayHandle play);
```

- `Stop`: 再生を完全に停止し、再生ハンドルを解放します。
- `Pause`: 再生を一時停止します。
- `Resume`: 一時停止を解除し、再生を再開します。
- いずれも成功時に `true` を返します。

### 3.8 パラメータ変更

```cpp
static bool SetVolume(PlayHandle play, float volume);
static bool SetPitch(PlayHandle play, float pitch);
```

- 再生中の音量・ピッチをリアルタイムに変更できます。
- `volume` は `0.0f` ～ `1.0f` にクランプされます。

### 3.9 状態問い合わせ

```cpp
static bool IsPlaying(PlayHandle play);
static bool IsPaused(PlayHandle play);
```

- `IsPlaying`: 再生中かどうかを返します（一時停止中は `false`）。
- `IsPaused`: 一時停止中かどうかを返します。

### 3.10 再生位置取得

```cpp
static bool GetPlayPositionSeconds(PlayHandle play, double& outSeconds);
```

- 再生中の音声の現在位置を秒単位で取得します。
- `SoundBeat` のビート検出に使用されます。

### 3.11 更新処理

```cpp
void Update();
```

- エンジンのメインループから毎フレーム呼び出されます。
- 再生終了した非ループ音声の自動クリーンアップを行います。
- 登録された `SoundBeat` と `AudioPlayer` の更新処理を呼び出します。

---

## 4. 使用例

### ファイル名で音声を取得して再生

```cpp
auto se = KashipanEngine::AudioManager::GetSoundHandleFromFileName("se_shot.wav");
auto play = KashipanEngine::AudioManager::Play(se, 0.8f);

// 後で停止
KashipanEngine::AudioManager::Stop(play);
```

### PlayParams を使用した詳細な再生

```cpp
KashipanEngine::AudioManager::PlayParams params;
params.sound = KashipanEngine::AudioManager::GetSoundHandleFromFileName("bgm.mp3");
params.volume = 0.5f;
params.pitch = 0.0f;
params.loop = true;
params.startTimeSec = 10.0;  // 10秒目から再生開始

auto play = KashipanEngine::AudioManager::Play(params);
```

### 再生状態の確認と操作

```cpp
auto play = KashipanEngine::AudioManager::Play(soundHandle);

if (KashipanEngine::AudioManager::IsPlaying(play)) {
    KashipanEngine::AudioManager::Pause(play);
}

if (KashipanEngine::AudioManager::IsPaused(play)) {
    KashipanEngine::AudioManager::Resume(play);
}

// 音量を徐々に下げる
KashipanEngine::AudioManager::SetVolume(play, 0.3f);
```
