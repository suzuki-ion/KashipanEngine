# ユーティリティ：時間（デルタタイム / 計測 / タイマー）

対象ヘッダ：
- `Utilities/TimeUtils.h`
- `Utilities/GameTimer.h`

---

## 1. TimeUtils（`Utilities/TimeUtils.h`）

### 1.1 目的

- `GameEngine` が更新するデルタタイム（秒）
- 現在時刻（ローカルタイム）
- 実行開始からの経過時間
- ラベル付き簡易計測
- ゲームスピード（倍率）の保持

を提供します。

### 1.2 デルタタイム

公開API：
- `void UpdateDeltaTime(Passkey<GameEngine>);`
- `float GetDeltaTime();`

厳密な仕様：
- `UpdateDeltaTime(...)` は `GameEngine` からのみ呼ばれます（パスキー）。
- 初回呼び出し時のデルタタイムは `0.0f` です（実装に `sIsFirstDeltaTimeCall` が存在）。

使用例（アプリ側・参照のみ）：

```cpp
#include <KashipanEngine.h>

float dt = KashipanEngine::GetDeltaTime();
```

### 1.3 ゲームスピード

公開API：
- `void SetGameSpeed(float speed);`
- `float GetGameSpeed();`

> `GetDeltaTime()` 自体に倍率が掛かるかどうかは実装契約に依存します。現状 `TimeUtils.cpp` には倍率変数があり、利用側がどう適用するかは呼び出し側設計になります。

### 1.4 現在時刻

公開API：
- `int GetNowTimeYear()` / `GetNowTimeMonth()` / `GetNowTimeDay()`
- `int GetNowTimeHour()` / `GetNowTimeMinute()`
- `long long GetNowTimeSecond()` / `GetNowTimeMillisecond()`
- `TimeRecord GetNowTime()`
- `std::string GetNowTimeString(const std::string &format = "%Y-%m-%d %H:%M:%S");`

厳密な仕様：
- 実装は `std::chrono::zoned_time` と `current_zone()` を使い、ローカルタイムとして扱います。

使用例：

```cpp
#include <KashipanEngine.h>

auto s = KashipanEngine::GetNowTimeString("%Y-%m-%d_%H-%M-%S");
```

### 1.5 実行時間（プログラム開始から）

公開API：
- `GetGameRuntimeYear/Month/Day/Hour/Minute/Second/Millisecond`
- `TimeRecord GetGameRuntime()`

厳密な仕様：
- 実装は起動時刻を `kProgramStartTime` として保持し、現在時刻との差で計算します。

### 1.6 簡易計測（ラベル）

公開API：
- `void StartTimeMeasurement(const std::string &label);`
- `TimeRecord EndTimeMeasurement(const std::string &label);`

厳密な仕様（実装より）：
- `label -> time_point` のマップを内部に保持します。
- `EndTimeMeasurement` は、該当ラベルが無い場合は 0 初期化の `TimeRecord` を返します。

使用例：

```cpp
#include <KashipanEngine.h>

KashipanEngine::StartTimeMeasurement("phase1");
// ... heavy work ...
auto rec = KashipanEngine::EndTimeMeasurement("phase1");
```

### 1.7 `KashipanEngine::GameTimer`（`TimeUtils.h` 版）

`TimeUtils.h` 内に `KashipanEngine::GameTimer` が定義されています。

特徴：
- `Update()` は内部で `GetDeltaTime()` を参照します（＝エンジンのデルタタイムと結びつく）。

主要API：
- `GameTimer(float duration, bool loop=false)`
- `void Update()`
- `void Start(float duration, bool loop=false)`
- `Stop/Reset/Pause/Resume`
- `IsActive/IsFinished`
- `GetProgress/GetReverseProgress/GetRemainingTime/GetElapsedTime`

使用例：

```cpp
#include <KashipanEngine.h>

KashipanEngine::GameTimer t(2.0f, false);
t.Start(2.0f);

// 毎フレーム
// t.Update();
// if (t.IsFinished()) { ... }
```

---

## 2. もう一つの `GameTimer`（`Utilities/GameTimer.h`）

`Utilities/GameTimer.h` の `GameTimer` は **グローバル名前空間**に定義されています。

特徴：
- `Update(float deltaTime = 1.0f / 60.0f)` の形で、呼び出し側がデルタタイムを与える設計です。

使用例：

```cpp
#include <KashipanEngine.h>

::GameTimer t(1.0f);

t.Start(1.0f);

// 毎フレーム
float dt = KashipanEngine::GetDeltaTime();
t.Update(dt);
```

### 注意点（厳密に）

- `KashipanEngine::GameTimer` と `::GameTimer` が同名で共存するため、
  - `KashipanEngine::GameTimer` と明示する
  - `::GameTimer` と明示する

など、曖昧性を避けてください。
