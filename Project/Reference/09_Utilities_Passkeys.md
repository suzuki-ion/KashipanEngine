# ユーティリティ：パスキー（アクセス制御）

対象ヘッダ：`Utilities/Passkeys.h`

---

## 1. 目的

「特定のクラス/関数からしか呼べないAPI」を、C++ の型システムだけで実現するための仕組みです。

- コンストラクタの呼び出し元制限
- 静的関数の呼び出し元制限

などで使われます。

---

## 2. 公開API

### 2.1 `Passkey<T>`

```cpp
template <typename T>
class Passkey final {
    friend T;
    Passkey() = default;
};
```

- `Passkey<T>` のコンストラクタは private 相当で、`friend T;` により `T` のみが生成できます。
- 生成できる側（`T`）が、そのパスキーを引数に取るAPIへアクセスできる、という構造です。

### 2.2 特殊パスキー

- `PasskeyForWinMain`：`WinMain` のみが生成可能
- `PasskeyForGameEngineMain`：`Execute(PasskeyForWinMain, ...)` のみが生成可能
- `PasskeyForCrashHandler`：`CrashHandler(...)` のみが生成可能

---

## 3. 使用例（コンストラクタを特定クラス限定にする）

```cpp
class Owner {
public:
    void Create() {
        Restricted r(Passkey<Owner>{});
        (void)r;
    }
};

class Restricted {
public:
    explicit Restricted(KashipanEngine::Passkey<Owner>) {}
};
```

この構造により、`Owner` 以外の場所から `Restricted` を構築できません。

---

## 4. エンジン内での例

例：`Window::Update(Passkey<GameEngine>)` のように
- `GameEngine` 以外から `Update` を呼べない

といった「責務の境界」を表現する用途で多用されます。
