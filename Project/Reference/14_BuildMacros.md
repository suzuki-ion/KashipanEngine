# ビルド時マクロ

KashipanEngine では、ビルド構成（Configuration）に応じてプリプロセッサマクロが定義されます。
これらのマクロを使用して、ビルド構成ごとに処理を切り替えることができます。

---

## ビルド構成マクロ

各ビルド構成で、以下のマクロが 1 つだけ定義されます。

- `DEBUG_BUILD` — Debug ビルド時に定義される
- `DEVELOPMENT_BUILD` — Development ビルド時に定義される
- `RELEASE_BUILD` — Release ビルド時に定義される

これらのマクロは排他的で、同時に複数が定義されることはありません。

### 例：ビルド構成による処理の切り替え

```cpp
#if defined(DEBUG_BUILD)
    // Debug ビルド専用の処理
    Log("デバッグモードです", LogSeverity::Debug);
#elif defined(DEVELOPMENT_BUILD)
    // Development ビルド専用の処理
    Log("開発モードです", LogSeverity::Info);
#elif defined(RELEASE_BUILD)
    // Release ビルド専用の処理
#endif
```

---

## `USE_IMGUI` マクロ

`USE_IMGUI` は **Debug ビルドおよび Development ビルドのときのみ** 定義されます。
Release ビルドでは定義されません。

このマクロは、ImGui を使用したデバッグ UI を有効化するために使用します。

- `DEBUG_BUILD` — `USE_IMGUI` が定義される
- `DEVELOPMENT_BUILD` — `USE_IMGUI` が定義される
- `RELEASE_BUILD` — `USE_IMGUI` は **定義されない**

### 例：ImGui によるデバッグ表示

```cpp
#if defined(USE_IMGUI)
#include <imgui.h>
#endif

void MyComponent::Update() {
    // 通常の更新処理
}

#if defined(USE_IMGUI)
void MyComponent::ShowImGui() {
    ImGui::Text("デバッグ情報を表示");
    ImGui::DragFloat("Speed", &speed_, 0.1f);
}
#endif
```

### 例：Debug / Development ビルドでのみテストシーンを登録

```cpp
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
#include "Scenes/TestScene.h"
#endif

void AppInitialize(const GameEngine::Context& context) {
    auto* sm = context.sceneManager;

    sm->RegisterScene<GameScene>("GameScene");

#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
    sm->RegisterScene<TestScene>("TestScene");
#endif
}
```

---

## 各ビルド構成の特徴まとめ

- **Debug** — デバッグ機能がすべて有効。ImGui 利用可能。最適化なし。開発中の主要ビルド構成。
- **Development** — ImGui 利用可能。ある程度の最適化あり。テストや調整に適したビルド構成。
- **Release** — デバッグ機能なし。ImGui 利用不可。最大限の最適化。配布用ビルド構成。

---

## 注意事項

- `USE_IMGUI` が定義されていない環境で ImGui のヘッダやAPIを使用するとコンパイルエラーになります。必ず `#if defined(USE_IMGUI)` で囲んでください。
- `ShowImGui()` などのデバッグ用メンバ関数は `#if defined(USE_IMGUI)` で宣言・定義を囲むのが慣例です。
- ビルド構成マクロはプロジェクト設定で自動的に定義されるため、手動で定義する必要はありません。
