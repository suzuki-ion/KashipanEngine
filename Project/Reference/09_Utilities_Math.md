# ユーティリティ：数学（`MathUtils`）

対象ヘッダ：`Utilities/MathUtils.h`

---

## 1. 目的

`MathUtils` は、
- ベクトル/行列の補助関数
- イージング
- ノイズ
- 球面座標系など

をまとめた「数学ユーティリティ」です。

`Utilities/MathUtils.h` は集約ヘッダであり、以下を取り込みます。

- `MathUtils/Easings.h`
- `MathUtils/Vector2.h`
- `MathUtils/Vector3.h`
- `MathUtils/Vector4.h`
- `MathUtils/Matrix3x3.h`
- `MathUtils/Matrix4x4.h`
- `MathUtils/PerlinNoise.h`
- `MathUtils/FractalNoise.h`
- `MathUtils/SphericalCoordinates.h`

加えて `M_PI` を定義します。

---

## 2. 名前空間

関数群は `KashipanEngine::MathUtils` に定義されます。

例：`Vector2` の補間/内積など（`Utilities/MathUtils/Vector2.h`）
- `Vector2 Lerp(const Vector2&, const Vector2&, float) noexcept;`
- `Vector2 Slerp(const Vector2&, const Vector2&, float) noexcept;`
- `constexpr float Dot(const Vector2&, const Vector2&) noexcept;`
- `float Length(const Vector2&) noexcept;`
- `Vector2 Normalize(const Vector2&);`

`Vector3` も同様に `Lerp/Slerp/Dot/Cross/Transform` 等を提供します。

---

## 3. 使用例（Vector の補助）

```cpp
#include <KashipanEngine.h>

Vector3 a{0.0f, 0.0f, 0.0f};
Vector3 b{10.0f, 0.0f, 0.0f};

Vector3 mid = KashipanEngine::MathUtils::Lerp(a, b, 0.5f);
float len = KashipanEngine::MathUtils::Length(mid);
(void)len;
```

## 4. 使用例（Catmull-Rom）

```cpp
#include <KashipanEngine.h>

std::vector<Vector2> points = {
    {0.0f, 0.0f},
    {1.0f, 2.0f},
    {3.0f, 3.0f},
    {4.0f, 0.0f},
};

Vector2 p = KashipanEngine::MathUtils::CatmullRomPosition(points, 0.25f, false);
```

---

## 5. 注意点

- ここでの `Vector2/Vector3/Vector4/Matrix*` は「数学型そのもの」ではなく、あくまで補助関数群です。
  - 型定義側（例：`KashipanEngine/Math/Vector3.h`）と併用します。
- `M_PI` を定義するため、他ライブラリとマクロ衝突する可能性があります。
