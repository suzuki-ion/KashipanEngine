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

