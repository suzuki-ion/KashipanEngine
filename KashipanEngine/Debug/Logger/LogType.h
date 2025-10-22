#pragma once

namespace KashipanEngine {

/// @brief ログの大分類
enum class LogDomain {
    GameEngine, // エンジン関連
    Application // ゲーム関連
};

/// @brief ログの情報の種類
enum class LogSeverity {
    Debug,      // デバッグ
    Info,       // 情報
    Warning,    // 警告
    Error,      // エラー
    Critical    // 致命的エラー
};

} // namespace KashipanEngine