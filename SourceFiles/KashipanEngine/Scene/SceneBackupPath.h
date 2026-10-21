#pragma once

namespace KashipanEngine {

/// @brief シーンの自動バックアップ（exe終了時の自動保存・エディターの定期自動保存）の保存先フォルダ
/// @details ゲームに同梱するAssetsフォルダへバックアップファイルが混ざらないよう、
///          Assets外の専用フォルダ（プロジェクトルート直下）に保存する。
///          値は論理パスで、実際の保存先は ProjectPaths::ToPhysical() を通して解決される
inline constexpr const char kSceneBackupDirectory[] = "SceneBackups/";

} // namespace KashipanEngine
