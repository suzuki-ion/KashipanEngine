# ユーティリティ：乱数（`RandomValue`）

対象ヘッダ：`Utilities/RandomValue.h`

---

## 1. 目的

ゲーム側で頻出する「範囲指定乱数」を簡易APIとして提供します。

---

## 2. 公開API

- `int GetRandomInt(int min, int max);`
  - `min` 以上 `max` 以下の整数乱数。

- `float GetRandomFloat(float min, float max);`
  - `min` 以上 `max` 以下の `float` 乱数。

- `double GetRandomDouble(double min, double max);`
  - `min` 以上 `max` 以下の `double` 乱数。

- `bool GetRandomBool(float trueProbability = 0.5f);`
  - `trueProbability`（0.0〜1.0）の確率で `true` を返す。

---

## 3. 使用例

```cpp
#include <KashipanEngine.h>

int r1 = KashipanEngine::GetRandomInt(0, 10);
float r2 = KashipanEngine::GetRandomFloat(-1.0f, 1.0f);
bool crit = KashipanEngine::GetRandomBool(0.1f); // 10%

(void)r1;
(void)r2;
(void)crit;
```

---

## 4. 注意点

- 乱数のシード・乱数エンジンの種類・スレッドセーフ性は実装（`RandomValue.cpp`）に従います。
- 確率的挙動の再現（リプレイ）を厳密に行いたい場合は、ゲーム側で乱数エンジンを別途管理する設計も検討してください。
