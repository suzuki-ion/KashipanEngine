#pragma once
#include <functional>
#include <memory>
#include <string>

namespace KashipanEngine::DevelopmentAI {
// リクエスト単位で認証するループバックMCPエンドポイント。通信処理からシーンには触れない。
// Pumpはエディタースレッドで呼び出す。Stopは終了待機の前に未処理リクエストを無効化する。
class EditorTools final {
public:
    explicit EditorTools(std::string catalog);
    ~EditorTools();
    EditorTools(const EditorTools &) = delete;
    EditorTools &operator=(const EditorTools &) = delete;
    const std::string &Url() const;
    const std::string &Token() const;
    void Pump(const std::function<std::string(const std::string &, const std::string &)> &handler);
    void Stop();
    void Revoke();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
