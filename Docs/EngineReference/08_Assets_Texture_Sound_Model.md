# テクスチャ / サウンド / モデルの取得方法

対象コード:

- `KashipanEngine/Assets/TextureManager.h/.cpp`
- `KashipanEngine/Assets/AudioManager.h/.cpp`
- `KashipanEngine/Assets/ModelManager.h/.cpp`
- `KashipanEngine/Objects/GameObjects/3D/Model.h`（利用例）

## 1. 共通方針

各マネージャは `GameEngine` から生成され、

- 起動時に Assets フォルダをスキャンしてロード
- または API で明示ロード

し、以降は **ハンドル（整数）** で参照します。

ハンドルの無効値:

- `TextureManager::kInvalidHandle`（0）
- `AudioManager::kInvalidSoundHandle`（0）
- `ModelManager::kInvalidHandle`（0）

## 2. TextureManager

### ロード

- `TextureManager::LoadTexture(filePath)`
  - `Assets` ルートからの相対 / フルパスのどちらも想定

### 取得（静的 API）

- `TextureManager::GetTexture(handle)`
- `TextureManager::GetTextureFromFileName(fileName)`
- `TextureManager::GetTextureFromAssetPath(assetPath)`

### シェーダへバインド

- `TextureManager::BindTexture(shaderBinder, nameKey, handle)`
- `TextureManager::BindTexture(shaderBinder, nameKey, IShaderTexture&)`

`IShaderTexture` として扱いたい場合は

- `TextureManager::TextureView view = TextureManager::GetTextureView(handle);`

を使用します。

## 3. AudioManager

### ロード

- `AudioManager::Load(filePath)`

### 取得（静的 API）

- `AudioManager::GetSoundHandleFromFileName(fileName)`
- `AudioManager::GetSoundHandleFromAssetPath(assetPath)`

### 再生

- `AudioManager::Play(soundHandle, volume, pitch, loop) -> PlayHandle`
- `AudioManager::Stop(playHandle)`
- `AudioManager::Pause(playHandle)`
- `AudioManager::Resume(playHandle)`
- `AudioManager::SetVolume(playHandle, volume)`
- `AudioManager::SetPitch(playHandle, pitch)`
- `AudioManager::IsPlaying(playHandle)`
- `AudioManager::IsPaused(playHandle)`

`AudioManager` インスタンス側には

- `void Update();`

があり、エンジン側が毎フレーム呼ぶ想定です。

## 4. ModelManager

### ロード

- `ModelManager::LoadModel(filePath)`

### 取得（静的 API）

ハンドル取得:

- `ModelManager::GetModelHandleFromFileName(fileName)`
- `ModelManager::GetModelHandleFromAssetPath(assetPath)`

モデルデータ取得:

- `ModelManager::GetModelData(handle) -> const ModelData&`
- `ModelManager::GetModelDataFromFileName(fileName)`
- `ModelManager::GetModelDataFromAssetPath(assetPath)`

`ModelData` はモデル生成に必要な頂点/インデックス/マテリアル情報を保持します。

### Model オブジェクトでの利用

`Objects/GameObjects/3D/Model` は以下の形で生成できます。

- `Model(const std::string &relativePath)`
- `Model(ModelManager::ModelHandle handle)`
- `Model(const ModelData &modelData)`

内部で `ModelManager` を利用するため、エンジン側で

- `Model::SetModelManager(Passkey<GameEngine>, ModelManager*)`

が呼ばれる前提の設計です。
