#pragma once
#ifdef USE_IMGUI
#include <string>

#include "Assets/AudioManager.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

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

} // namespace KashipanEngine

#endif // USE_IMGUI
