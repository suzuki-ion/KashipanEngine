#pragma once

#include <cstdint>
#include <memory>

class asIScriptContext;

namespace KashipanEngine {

/// @brief VS Code等のDAPクライアントからAngelScriptをデバッグするための最小デバッグサーバー
/// @details 技術検証版として、アタッチ、行ブレークポイント、Pause/Continue、基本的な
///          ステップ実行、コールスタックと変数の表示をサポートする。localhostからのみ接続を受け付ける。
class AngelScriptDebugServer final {
public:
    AngelScriptDebugServer();
    ~AngelScriptDebugServer();

    AngelScriptDebugServer(const AngelScriptDebugServer &) = delete;
    AngelScriptDebugServer &operator=(const AngelScriptDebugServer &) = delete;
    AngelScriptDebugServer(AngelScriptDebugServer &&) = delete;
    AngelScriptDebugServer &operator=(AngelScriptDebugServer &&) = delete;

    /// @brief DAPサーバーを開始する
    /// @return 待受開始に成功した場合はtrue
    bool Start(std::uint16_t port = 27979);

    /// @brief DAPサーバーを停止し、停止中のスクリプトがあれば再開する
    void Stop();

    /// @brief AngelScriptコンテキストへデバッグ用ラインコールバックを設定する
    void AttachContext(asIScriptContext *context) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief Debug/Developmentビルド内でゲームスクリプトとEditorToolが共有するDAPサーバーを取得する
AngelScriptDebugServer &GetProcessAngelScriptDebugServer();

} // namespace KashipanEngine
