#pragma once

namespace KashipanEngine {

class GraphicsEngine;

class Renderer final {
public:
    Renderer(Passkey<GraphicsEngine>) {}
    ~Renderer() = default;
    
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void RenderFrame(Passkey<GraphicsEngine>);
};

} // namespace KashipanEngine
