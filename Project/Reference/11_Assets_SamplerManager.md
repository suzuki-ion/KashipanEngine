# アセット管理：サンプラー（`SamplerManager`）

対象ヘッダ：`KashipanEngine/Assets/SamplerManager.h`

---

## 1. 目的

D3D12 サンプラーの作成・管理・シェーダーへのバインドを一括して行うクラスです。
D3D12 のサンプラーハンドルを外部へ直接公開せず、ハンドルベースの抽象化を提供します。

---

## 2. 設計概要

- サンプラーは `SamplerHandle`（`uint32_t`）で管理します。
- 無効ハンドルは `SamplerManager::kInvalidHandle`（`0`）です。
- エンジン初期化時に 6 種類のデフォルトサンプラーが自動作成されます。
- サンプラーの同一設定による重複排除（dedup）は行いません。

---

## 3. `DefaultSampler`（既定サンプラー列挙型）

```cpp
enum class DefaultSampler {
    PointClamp = 0,
    PointWrap = 1,
    LinearClamp = 2,
    LinearWrap = 3,
    AnisotropicClamp = 4,
    AnisotropicWrap = 5,
};
```

エンジン初期化時に以下のサンプラーが自動的に作成されます。

- `PointClamp` - 最近傍補間 + クランプ
- `PointWrap` - 最近傍補間 + ラップ
- `LinearClamp` - バイリニア補間 + クランプ
- `LinearWrap` - バイリニア補間 + ラップ
- `AnisotropicClamp` - 異方性フィルタリング（16x）+ クランプ
- `AnisotropicWrap` - 異方性フィルタリング（16x）+ ラップ

---

## 4. 公開API

### 4.1 型定義・定数

```cpp
using SamplerHandle = uint32_t;
static constexpr SamplerHandle kInvalidHandle = 0;
```

### 4.2 コンストラクタ（`GameEngine` からのみ生成可能）

```cpp
SamplerManager(Passkey<GameEngine>, DirectXCommon* directXCommon);
```

### 4.3 サンプラー作成

```cpp
static SamplerHandle CreateSampler(const D3D12_SAMPLER_DESC& desc);
```

- `D3D12_SAMPLER_DESC` を指定してカスタムサンプラーを作成します。
- 同一設定であっても毎回新しいハンドルが割り当てられます。
- 失敗時は `kInvalidHandle` を返します。

### 4.4 バインド

```cpp
static bool BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, SamplerHandle handle);
static bool BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, DefaultSampler defaultSampler);
```

- シェーダー変数バインダー経由でサンプラーをシェーダーにバインドします。
- `nameKey` はシェーダーステージとスロット名の組み合わせ（例：`"Pixel:gSampler"`）を指定します。
- `DefaultSampler` オーバーロードでは、列挙値に対応する既定サンプラーがバインドされます。

---

## 5. 使用例

### 既定サンプラーをバインド

```cpp
KashipanEngine::SamplerManager::BindSampler(&binder, "Pixel:gSampler", KashipanEngine::DefaultSampler::LinearWrap);
```

### カスタムサンプラーを作成してバインド

```cpp
D3D12_SAMPLER_DESC desc{};
desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
desc.MaxLOD = D3D12_FLOAT32_MAX;
desc.MaxAnisotropy = 1;

auto sampler = KashipanEngine::SamplerManager::CreateSampler(desc);
KashipanEngine::SamplerManager::BindSampler(&binder, "Pixel:gSampler", sampler);
```

### 各 DefaultSampler の選び方

- **ピクセルアートなど、ぼかさずに拡大したい場合** → `PointClamp` または `PointWrap`
- **一般的な3Dテクスチャに滑らかなフィルタリングが必要な場合** → `LinearWrap`
- **テクスチャの端でリピートさせたくない場合** → `LinearClamp`
- **高品質な異方性フィルタリングが必要な場合** → `AnisotropicWrap` または `AnisotropicClamp`
