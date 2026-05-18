# アセット管理：音声プレイヤー（`AudioPlayer`）

対象ヘッダ：`KashipanEngine/Assets/AudioPlayer.h`

関連：`11_Assets_AudioManager.md`

---

## 1. 目的

複数の音声をプレイリストとして管理し、クロスフェードによる切り替えをサポートする音声プレイヤーです。
BGM の切り替えなど、シームレスな音声遷移が必要な場面で使用します。

---

## 2. 設計概要

- `AudioManager` の上位レイヤーとして動作します。
- 内部に `AudioManager::PlayParams` のリスト（プレイリスト）を保持します。
- クロスフェード時は現在再生中の音声をフェードアウトしつつ、次の音声をフェードインします。
- `AudioManager::Update()` から自動的に `AudioPlayer::Update()` が呼び出されます（`AudioManager` に自動登録）。
- コピー・ムーブは禁止されています。

---

## 3. 公開API

### 3.1 型定義・定数

```cpp
using SoundHandle = AudioManager::SoundHandle;
using PlayHandle = AudioManager::PlayHandle;
static constexpr size_t kInvalidAudioIndex = std::numeric_limits<size_t>::max();
```

### 3.2 コンストラクタ・デストラクタ

```cpp
AudioPlayer();
~AudioPlayer();
```

- コンストラクタで `AudioManager` に自動登録されます。
- デストラクタで自動登録解除および再生中の音声を停止します。

### 3.3 プレイリスト管理

```cpp
void AddAudio(SoundHandle sound);
void AddAudio(const AudioManager::PlayParams& params);
void AddAudios(const std::vector<SoundHandle>& sounds);
void AddAudios(const std::vector<AudioManager::PlayParams>& paramsList);
void RemoveAudio(SoundHandle sound);
void RemoveAudios(const std::vector<SoundHandle>& sounds);
```

- `AddAudio`: プレイリストに音声を追加します。`SoundHandle` のみの場合はデフォルトパラメータで追加されます。
- `AddAudios`: 複数の音声を一括追加します。
- `RemoveAudio`: プレイリストから指定の音声を削除します。再生中の場合は停止されます。
- `RemoveAudios`: 複数の音声を一括削除します。

### 3.4 音声切り替え

```cpp
bool ChangeAudio(double crossFadeSec, size_t changeAudioIndex = kInvalidAudioIndex);
bool ChangeAudio(double crossFadeSec, const AudioManager::PlayParams& params, size_t changeAudioIndex = kInvalidAudioIndex);
```

- `crossFadeSec`: クロスフェードの時間（秒）。`0` 以下の場合は即座に切り替えます。
- `changeAudioIndex`: 切り替え先のプレイリスト内インデックス。`kInvalidAudioIndex` の場合は次の曲へ進みます。
- `params` オーバーロード: 再生パラメータを上書きして切り替えます。
- 成功時に `true` を返します。

### 3.5 状態取得

```cpp
size_t GetCurrentAudioIndex() const noexcept;
PlayHandle GetCurrentPlayHandle() const noexcept;
```

- `GetCurrentAudioIndex`: 現在再生中のプレイリスト内インデックスを返します。
- `GetCurrentPlayHandle`: 現在再生中の `PlayHandle` を返します。

---

## 4. クロスフェードの動作

1. `ChangeAudio()` が呼ばれると、次の音声が音量 `0` で再生開始されます。
2. 毎フレームの `Update()` で経過時間に応じて音量が補間されます。
  - 現在の音声：`(1 - t) * currentVolume` でフェードアウト
  - 次の音声：`t * nextVolume` でフェードイン
3. クロスフェードが完了すると、現在の音声が停止され、次の音声が現在の音声に昇格します。

---

## 5. 使用例

### BGM をプレイリストで管理し、クロスフェードで切り替え

```cpp
// AudioPlayer のインスタンスを作成
KashipanEngine::AudioPlayer player;

// プレイリストに BGM を追加
auto bgm1 = KashipanEngine::AudioManager::GetSoundHandleFromFileName("bgm_title.mp3");
auto bgm2 = KashipanEngine::AudioManager::GetSoundHandleFromFileName("bgm_battle.mp3");
player.AddAudio(bgm1);
player.AddAudio(bgm2);

// 最初の曲を再生開始（即座に切り替え）
player.ChangeAudio(0.0);

// 2秒かけて次の曲へクロスフェード
player.ChangeAudio(2.0);
```

### PlayParams を使用した切り替え

```cpp
KashipanEngine::AudioManager::PlayParams params;
params.volume = 0.7f;
params.loop = true;

// インデックス 1 の音声に、指定パラメータでクロスフェード切り替え
player.ChangeAudio(3.0, params, 1);
```
