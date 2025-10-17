#pragma once

namespace KashipanEngine {

/// @brief ログの大分類
enum class LogDomain {
    Engine,         // エンジン関連
    Game            // ゲーム関連
};

/// @brief ログの中分類
enum class LogCategory {
    Core,           // コア関連
    Graphics,       // グラフィックス関連
    Texture,        // テクスチャ関連
    Audio,          // オーディオ関連
    Input,          // 入力関連
    Resource,       // リソース関連
    Scripting,      // スクリプト関連
    Physics,        // 物理演算関連
    Gameplay,       // ゲームプレイ関連
    AI,             // AI関連
    UI,             // ユーザーインターフェース関連
    Animation,      // アニメーション関連
    Scene,          // シーン管理関連
    Debug,          // デバッグ関連
    Performance,    // パフォーマンス関連
    Object,         // オブジェクト関連
    Other           // その他
};

/// @brief ログの情報の種類
enum class LogSeverity {
    Info,           // 情報
    Warning,        // 警告
    Error           // エラー
};

/// @brief ログの内容の種類
enum class LogContentType {
    Message,        // 通常メッセージ
    Assertion,      // アサーション
    Exception,      // 例外
    Performance,    // パフォーマンス関連
    Allocation,     // メモリ割り当て関連
    StateChange,    // 状態変更関連
    Event,          // イベント関連
    Other           // その他
};

} // namespace KashipanEngine