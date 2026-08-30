#pragma once
#ifdef USE_IMGUI
#include <string>

#include "Assets/AudioManager.h"
#include "Assets/GifManager.h"
#include "Assets/GifPlayer.h"
#include "Assets/ModelManager.h"
#include "Assets/VideoManager.h"
#include "Assets/VideoPlayer.h"
#include "Math/Vector3.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class SceneContext;

/// @brief .jsonファイルをツリー構造で編集するウィンドウ
/// @details オブジェクト/配列の要素追加・削除、値の編集をシーンオブジェクトのヒエラルキーに近い
///          操作感（TreeNode + ボタン）で行える。編集内容は明示的に「Save」を押すまで実ファイルへは反映されない。
class JSONFileEditorWindow final {
public:
    explicit JSONFileEditorWindow(const std::string &filePath);
    ~JSONFileEditorWindow() = default;

    /// @brief 毎フレーム呼ぶ。falseを返したらウィンドウが閉じられたので呼び出し元のリストから取り除くこと
    bool ShowImGui();
    const std::string &GetFilePath() const noexcept { return filePath_; }

private:
    void ShowNode(const std::string &label, JSON &node);
    void ShowAddMemberRow(JSON &objectNode);
    void ShowAddElementRow(JSON &arrayNode);

    std::string filePath_;
    std::string windowTitle_;
    JSON data_;
    bool isOpen_ = true;
    bool isDirty_ = false;
    bool loadFailed_ = false;
};

/// @brief .matファイルをMaterialManagerのマテリアル管理ImGuiと同じ要領で編集するウィンドウ
class MaterialFileEditorWindow final {
public:
    explicit MaterialFileEditorWindow(const std::string &assetPath);
    ~MaterialFileEditorWindow() = default;

    bool ShowImGui();
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;
};

/// @brief 画像ファイルのプレビュー用ウィンドウ
class ImagePreviewWindow final {
public:
    explicit ImagePreviewWindow(const std::string &assetPath);
    ~ImagePreviewWindow() = default;

    bool ShowImGui();
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;
};

/// @brief GIFアニメーションの再生プレビュー用ウィンドウ
/// @details フレーム送り・テクスチャのライフサイクル管理は`GifSource`コンポーネントと共通の
///          `GifPlayer`に委譲する。`Update()`はScene側のコンポーネント更新には乗らないため、
///          毎フレーム`ShowImGui()`内で`GifPlayer::Update()`を呼び自前で進行させる。
class GifPreviewWindow final {
public:
    explicit GifPreviewWindow(const std::string &assetPath);
    ~GifPreviewWindow();

    bool ShowImGui();
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;
    GifManager::GifHandle gifHandle_ = GifManager::kInvalidHandle;
    std::unique_ptr<GifPlayer> player_;
    bool loop_ = true;
};

/// @brief 音声ファイルの再生プレビュー用ウィンドウ
class AudioPreviewWindow final {
public:
    explicit AudioPreviewWindow(const std::string &assetPath);
    ~AudioPreviewWindow();

    bool ShowImGui();
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;
    AudioManager::SoundHandle soundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::PlayHandle playHandle_ = AudioManager::kInvalidPlayHandle;
    float volume_ = 1.0f;
};

/// @brief 動画ファイルの再生プレビュー用ウィンドウ
class VideoPreviewWindow final {
public:
    explicit VideoPreviewWindow(const std::string &assetPath);
    ~VideoPreviewWindow();

    bool ShowImGui();
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;
    VideoManager::VideoHandle videoHandle_ = VideoManager::kInvalidHandle;
    VideoPlayer *player_ = nullptr;
    bool loop_ = true;
    float volume_ = 1.0f;
};

/// @brief モデルファイルの3Dプレビュー用ウィンドウ（オービットカメラで表示）
/// @details 現在編集中のシーンに非表示・非保存のヘルパーオブジェクト（ScreenBuffer/Camera/Light/Mesh）を
///          生成し、既存の描画パイプラインでプレビュー専用のScreenBufferへ描画させる
class ModelPreviewWindow final {
public:
    explicit ModelPreviewWindow(const std::string &assetPath);
    ~ModelPreviewWindow() = default;

    /// @brief 毎フレーム呼ぶ。falseを返したらウィンドウが閉じられたので呼び出し元のリストから取り除くこと
    /// @param sceneContext プレビュー用オブジェクトの生成先（現在編集中のシーン）
    bool ShowImGui(SceneContext *sceneContext);
    const std::string &GetAssetPath() const noexcept { return assetPath_; }

private:
    bool EnsurePreviewObjects(SceneContext *sceneContext);
    void DestroyPreviewObjects(SceneContext *sceneContext);
    void ApplyModelToPreview(SceneContext *sceneContext);
    void UpdateCameraTransform(SceneContext *sceneContext);
    void HandleCameraInput();

    std::string assetPath_;
    std::string windowTitle_;
    bool isOpen_ = true;

    ModelManager::ModelHandle modelHandle_ = ModelManager::kInvalidHandle;
    bool applied_ = false;

    // 生ポインタはフレームをまたいで保持せず、毎回SceneContext::GetSceneObjectでUUIDから引き直す
    UUID128 targetObjectID_;
    UUID128 cameraObjectID_;
    UUID128 lightObjectID_;
    UUID128 meshObjectID_;
    bool objectsValid_ = false;

    float yaw_ = 0.6f;
    float pitch_ = 0.35f;
    float distance_ = 5.0f;
    Vector3 target_{ 0.0f, 0.0f, 0.0f };
};

} // namespace KashipanEngine

#endif // USE_IMGUI
