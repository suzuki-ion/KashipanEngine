# `Assets` フォルダと各種マネージャ（Texture/Model/Sampler/Audio）

KashipanEngine は `GameEngine` 初期化時に各種アセットマネージャを生成し、`SceneBase` から静的アクセサで参照できるように設定します。

- `TextureManager`
- `SamplerManager`
- `ModelManager`
- `AudioManager`

関連：`KashipanEngine/AssetsHeaders.h`、`KashipanEngine/Assets/*`、`KashipanEngine/Core/GameEngine.cpp`

---

## 重要：`Assets` 配下のファイルは「起動時に読み込まれる」

各マネージャ（`TextureManager` / `ModelManager` / `AudioManager`）は内部に `LoadAllFromAssetsFolder()` を持ち、
**Assets ルート以下を起動時に走査して事前ロード**する設計になっています。

そのため、基本運用は以下です。

- ゲームで使うファイルを `Assets/` 配下へ配置しておく
- 実行時は「ファイル名」または「Assets ルートからの相対パス」でハンドルを取得して使う

> 事前ロードの対象拡張子や探索ルールは各 `*Manager.cpp` の実装に従います。

---

## 公開API（入口）

### `SceneBase`（エンジン資産への静的アクセサ）
- `static AudioManager* GetAudioManager()`
- `static ModelManager* GetModelManager()`
- `static SamplerManager* GetSamplerManager()`
- `static TextureManager* GetTextureManager()`

これらのポインタは `GameEngine` が `SceneBase::SetEnginePointers(...)` で設定します。

---

## `TextureManager`（テクスチャ）

ヘッダ：`KashipanEngine/Assets/TextureManager.h`

### 設計
- テクスチャは `TextureHandle`（`uint32_t`）で管理します。
- 外部へ D3D12 の SRV ハンドルを直接渡さず、必要に応じて `BindTexture(...)` を通してバインドします。

### 公開API（抜粋）
- `TextureHandle LoadTexture(const std::string& filePath)`
  - `Assets` ルート相対パス or フルパスでロード
- 取得
  - `static TextureHandle GetTexture(TextureHandle handle)`
  - `static TextureHandle GetTextureFromFileName(const std::string& fileName)`
  - `static TextureHandle GetTextureFromAssetPath(const std::string& assetPath)`
- ビュー
  - `static TextureView GetTextureView(TextureHandle handle)`（`IShaderTexture` として扱える）
- バインド
  - `static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, TextureHandle handle)`
  - `static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, const IShaderTexture& texture)`

### 例：ファイル名から取得してバインド

```cpp
auto tex = KashipanEngine::TextureManager::GetTextureFromFileName("player.png");

// binder は Object の Render() / BindShaderVariables() 等から渡される想定
// TextureManager::BindTexture(&binder, "Pixel:gDiffuseTexture", tex);
```

---

## `ModelManager`（モデル）

ヘッダ：`KashipanEngine/Assets/ModelManager.h`

### 設計
- モデルは `ModelHandle`（`uint32_t`）で管理します。
- `ModelData` はモデル生成用データ（頂点/インデックス/マテリアル）を保持します。

### 公開API（抜粋）
- `ModelHandle LoadModel(const std::string& filePath)`
- 取得
  - `static ModelHandle GetModelHandleFromFileName(const std::string& fileName)`
  - `static ModelHandle GetModelHandleFromAssetPath(const std::string& assetPath)`
  - `static const ModelData& GetModelData(ModelHandle handle)`
  - `static const ModelData& GetModelDataFromFileName(const std::string& fileName)`
  - `static const ModelData& GetModelDataFromAssetPath(const std::string& assetPath)`

### 例：ファイル名から `ModelData` を参照

```cpp
auto h = KashipanEngine::ModelManager::GetModelHandleFromFileName("enemy.fbx");
const auto& data = KashipanEngine::ModelManager::GetModelData(h);

uint32_t vtx = data.GetVertexCount();
uint32_t idx = data.GetIndexCount();
(void)vtx;
(void)idx;
```

---

## `AudioManager`（音声）

ヘッダ：`KashipanEngine/Assets/AudioManager.h`

### 設計
- 音声（リソース）は `SoundHandle`
- 再生（インスタンス）は `PlayHandle`

### 公開API（抜粋）
- `SoundHandle Load(const std::string& filePath)`
- 取得
  - `static SoundHandle GetSoundHandleFromFileName(const std::string& fileName)`
  - `static SoundHandle GetSoundHandleFromAssetPath(const std::string& assetPath)`
- 再生制御
  - `static PlayHandle Play(SoundHandle sound, float volume = 1.0f, float pitch = 0.0f, bool loop = false)`
  - `static bool Stop(PlayHandle play)`
  - `static bool Pause(PlayHandle play)` / `static bool Resume(PlayHandle play)`
  - `static bool SetVolume(PlayHandle play, float volume)`
  - `static bool SetPitch(PlayHandle play, float pitch)`
  - `static bool IsPlaying(PlayHandle play)` / `static bool IsPaused(PlayHandle play)`

### 例：ファイル名で音声を引いて再生

```cpp
auto se = KashipanEngine::AudioManager::GetSoundHandleFromFileName("se_shot.wav");
auto play = KashipanEngine::AudioManager::Play(se, 0.8f);

// 後で停止
KashipanEngine::AudioManager::Stop(play);
```

---

## `SamplerManager`（サンプラー）

ヘッダ：`KashipanEngine/Assets/SamplerManager.h`

### 設計
- サンプラーは `SamplerHandle`（`uint32_t`）で管理し、外部へ D3D12 ハンドルを出しません。
- 既定サンプラーは `DefaultSampler`（`PointClamp` / `LinearWrap` 等）として利用できます。

### 公開API（抜粋）
- `static SamplerHandle CreateSampler(const D3D12_SAMPLER_DESC& desc)`
- `static bool BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, SamplerHandle handle)`
- `static bool BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, DefaultSampler defaultSampler)`

### 例：既定サンプラーをバインド

```cpp
// SamplerManager::BindSampler(&binder, "Pixel:gSampler", KashipanEngine::DefaultSampler::LinearWrap);
```

---

## `Assets` の配置（実務上のルール）

- `Assets/` 直下〜配下に配置したファイルがロード対象
- engine 付属のデータ（パイプライン定義、エンジンロゴ等）は `Assets/KashipanEngine/...` に配置されている想定

例：
- `Assets/KashipanEngine/EngineSettings.json`
- `Assets/KashipanEngine/Pipeline/...`（パイプライン定義やプリセット）

> `EngineSettings.json` の内容は `GameEngine` 起動時に読み込まれ、ウィンドウ初期パラメータ等に反映されます。

## ファイル名指定時について

> ファイル名指定時の文字の大文字小文字は、Windowsの仕様に則って関係しないようにしています。