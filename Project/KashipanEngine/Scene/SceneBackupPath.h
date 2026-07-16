#pragma once

namespace KashipanEngine {

/// @brief シーンの自動バックアップ（exe終了時の自動保存・エディターの定期自動保存）の保存先フォルダ
/// @details ゲームに同梱するAssetsフォルダへバックアップファイルが混ざらないよう、
///          Assets外の専用フォルダ（実行ディレクトリ直下）に保存する
inline constexpr const char kSceneBackupDirectory[] = "SceneBackups/";

} // namespace KashipanEngine
