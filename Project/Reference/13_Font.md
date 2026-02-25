# フォント / テキスト描画（`Font`）

フォント関連は `KashipanEngine/FontHeaders.h` で一括インクルードできます。

```cpp
#include <KashipanEngine.h>
// もしくは
#include "KashipanEngine/FontHeaders.h"
```

---

## 構成

- `FontLoader`：`.fnt`（BMFont形式）を読み込み `FontData` を生成
- `Text`：`Sprite` を文字単位で内部生成し、フォント情報に基づいてレイアウトして描画

---

## `Text` の基本的な使い方

### 生成と描画登録

`Text` は内部的に文字数分の `Sprite` を持つため、コンストラクタで最大文字数（確保数）を指定します。

```cpp
auto text = std::make_unique<Text>(128);
text->SetName("UI_Text");

if (auto* tr = text->GetComponent2D<Transform2D>()) {
    tr->SetTranslate(Vector2(120.0f, 120.0f));
}

text->SetFont("Assets/Fonts/TestFont.fnt");
text->SetText(u8"Hello, KashipanEngine!\nText Rendering Test");
text->SetTextAlign(TextAlignX::Left, TextAlignY::Top);

text->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
AddObject2D(std::move(text));
```

### テキスト更新

`SetText(...)` を呼ぶと、内部レイアウトを再構築します。

```cpp
text->SetText(u8"Score: 100");
// u8string型でなくても通常のstring型も受け付けます。
text->SetText("Time: 12.34");
```

### テキスト更新（`std::format` スタイル）

`SetTextFormat(...)` で `std::format` と同様に `{}` プレースホルダーへ値を埋め込めます。

```cpp
int score = 100;
float time = 12.34f;
text->SetTextFormat("Score: {}  Time: {:.2f}", score, time);
```

> `SetTextFormat(...)` の format は `std::format_string<...>` のため、
> 文字列リテラル（またはコンパイル時に妥当性が取れる形式）での使用を推奨します。

---

## 文字ごとの `Sprite` へのアクセス（`operator[]`）

`Text` は内部的に文字単位の `Sprite` を保持しており、`operator[]` でアクセスできます。

- `text[i]` は i 番目の文字スプライト（`Sprite*`）を返します
- 範囲外の場合は `nullptr` を返します

これにより、文字ごとに色・座標・拡大率などを個別に調整できます。

例：先頭の1文字だけ色を変える

```cpp
if (auto* s = (*text)[0]) {
    if (auto* mat = s->GetComponent2D<Material2D>()) {
        mat->SetColor(Vector4(1.0f, 0.2f, 0.2f, 1.0f));
    }
}
```

---

## 揃え（アライン）

- `SetTextAlign(TextAlignX, TextAlignY)` で行単位の揃えを指定します。

```cpp
text->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
```

---

## 参照（関連）

- `02_GameObjects.md`（`Object2DBase` / `Sprite`）
- `11_Assets_TextureManager.md`（テクスチャ管理：フォントページの解決で利用）

