#pragma once
#include <functional>
#include <memory>
#include <string>

namespace KashipanEngine::DevelopmentAI {
// Request-scoped, authenticated loopback MCP endpoint. Networking never touches the scene.
// Pump is called by the editor thread; Stop revokes pending requests before joining.
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
