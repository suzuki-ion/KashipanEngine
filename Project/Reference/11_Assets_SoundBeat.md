# アセット管理：ビート検出（`SoundBeat`）

対象ヘッダ：`KashipanEngine/Assets/SoundBeat.h`

関連：`11_Assets_AudioManager.md`

---

## 1. 目的

再生中の音声に対して BPM（テンポ）ベースのビート検出を行うクラスです。
リズムゲームや音楽同期演出など、ビートに合わせた処理を実現します。

---

## 2. 設計概要

- `AudioManager` の再生ハンドル（`PlayHandle`）と BPM を組み合わせてビートタイミングを計算します。
- `AudioManager::Update()` から自動的に `SoundBeat::Update()` が呼び出されます（`AudioManager` に自動登録）。
- ビートが発生するとコールバックを呼び出し、`IsOnBeatTriggered()` が `true` を返します。
- 再生ハンドルなしでも「手動ビートモード」で動作可能です。
- コピー・ムーブは禁止されています。

---

## 3. 公開API

### 3.1 型定義

```cpp
using PlayHandle = AudioManager::PlayHandle;
```

### 3.2 コンストラクタ・デストラクタ

```cpp
SoundBeat();
SoundBeat(PlayHandle play, float bpm, double startOffsetSec);
~SoundBeat();
```

- デフォルトコンストラクタ：非アクティブ状態（BPM = `0`）で作成されます。
- パラメータ付きコンストラクタ：再生ハンドル・BPM・開始オフセットを指定して即座にアクティブになります。
- いずれも `AudioManager` に自動登録されます。

### 3.3 ビート設定

```cpp
void SetBeat(PlayHandle play, float bpm, double startOffsetSec);
void SetPlayHandle(PlayHandle play) noexcept;
void SetBPM(float bpm) noexcept;
void SetStartOffsetSec(double startOffsetSec) noexcept;
```

- `SetBeat`: 再生ハンドル・BPM・開始オフセットを一括設定します。ビートカウントもリセットされます。
- `SetPlayHandle`: 再生ハンドルのみ変更します。
- `SetBPM`: BPM のみ変更します。
- `SetStartOffsetSec`: 開始オフセットのみ変更します。

### 3.4 コールバック

```cpp
void SetOnBeat(std::function<void(PlayHandle, uint64_t, double)> cb);
```

- ビートが検出されるたびに呼ばれるコールバックを設定します。
- 引数は `(再生ハンドル, ビートインデックス, 現在の再生位置（秒）)` です。

### 3.5 状態取得

```cpp
float GetBeatProgress() const;
uint64_t GetCurrentBeat() const noexcept;
bool IsActive() const noexcept;
bool IsOnBeatTriggered() const noexcept;
```

- `GetBeatProgress`: 現在のビート区間内の進行度を `0.0f` ～ `1.0f` で返します。
- `GetCurrentBeat`: 現在のビートインデックス（0始まり）を返します。
- `IsActive`: BPM が正の値であればアクティブです。
- `IsOnBeatTriggered`: 直前の `Update()` でビートが発生したかどうかを返します。

### 3.6 リセット

```cpp
void Reset();
```

- ビートカウントをリセットします。手動ビートモードの場合は開始時刻もリセットされます。

### 3.7 手動ビートモード

```cpp
void StartManualBeat();
void StopManualBeat();
```

- `StartManualBeat`: 再生ハンドルなし（`kInvalidPlayHandle`）で、現在時刻を基準にビート検出を開始します。
- `StopManualBeat`: 手動ビートモードを停止します。

> 手動ビートモードは、音声を再生せずにテンポだけで同期処理をしたい場合に使用します。

---

## 4. ビート検出の仕組み

1. 毎フレームの `Update()` で再生位置（秒）を取得します。
2. `(再生位置 - 開始オフセット) / (60.0 / BPM)` でビートインデックスを算出します。
3. 前回のビートインデックスと異なる場合、ビートが発生したと判定します。
4. コールバックが設定されていれば呼び出し、`IsOnBeatTriggered()` を `true` にします。

---

## 5. 使用例

### 基本的なビート検出

```cpp
// 音声を再生
auto bgm = KashipanEngine::AudioManager::GetSoundHandleFromFileName("bgm.mp3");
auto play = KashipanEngine::AudioManager::Play(bgm, 1.0f, 0.0f, true);

// SoundBeat を設定（120 BPM、オフセットなし）
KashipanEngine::SoundBeat beat(play, 120.0f, 0.0);
```

### コールバックでビートごとに処理

```cpp
KashipanEngine::SoundBeat beat;
beat.SetBeat(playHandle, 140.0f, 0.5);
beat.SetOnBeat([](KashipanEngine::AudioManager::PlayHandle ph, uint64_t beatIndex, double posSec) {
    // ビートごとに実行される処理
    (void)ph;
    (void)beatIndex;
    (void)posSec;
});
```

### ビート進行度を使った演出

```cpp
// 毎フレームの更新処理内で
float progress = beat.GetBeatProgress();
// progress: 0.0（ビート直後）～ 1.0（次のビート直前）
// この値を使ってスケーリングや色変化などの演出に利用
```

### 手動ビートモード

```cpp
KashipanEngine::SoundBeat beat;
beat.SetBPM(100.0f);
beat.StartManualBeat();

// 毎フレーム、IsOnBeatTriggered() でビートを検出可能
if (beat.IsOnBeatTriggered()) {
    // ビート発生時の処理
}
```
