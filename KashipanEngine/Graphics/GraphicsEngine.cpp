#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/2D/Ellipse.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"

#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Box.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/Translation.h"
#endif

#include <unordered_map>
#include <chrono>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace KashipanEngine {

namespace {
constexpr bool kEnableInstancingTest = true;

// Number of objects to generate for instancing verification
constexpr std::uint32_t kInstancingTestCount2D = 1024;
constexpr std::uint32_t kInstancingTestCount3D = 1024;

#if defined(USE_IMGUI)
class RollingAverage {
public:
    explicit RollingAverage(std::size_t capacity = 60) { SetCapacity(capacity); }

    void SetCapacity(std::size_t capacity) {
        capacity = std::max<std::size_t>(1, capacity);
        if (capacity_ == capacity) return;

        capacity_ = capacity;
        samples_.clear();
        samples_.reserve(capacity_);
        writeIndex_ = 0;
        sum_ = 0.0;
    }

    void Add(double value) {
        if (samples_.size() < capacity_) {
            samples_.push_back(value);
            sum_ += value;
            return;
        }

        sum_ -= samples_[writeIndex_];
        samples_[writeIndex_] = value;
        sum_ += value;

        writeIndex_ = (writeIndex_ + 1) % capacity_;
    }

    double GetAverage() const {
        if (samples_.empty()) return 0.0;
        return sum_ / static_cast<double>(samples_.size());
    }

    std::size_t GetCapacity() const { return capacity_; }
    std::size_t GetCount() const { return samples_.size(); }

private:
    std::vector<double> samples_{};
    std::size_t capacity_ = 1;
    std::size_t writeIndex_ = 0;
    double sum_ = 0.0;
};
#endif

} // namespace

GraphicsEngine::GraphicsEngine(Passkey<GameEngine>, DirectXCommon *directXCommon)
    : directXCommon_(directXCommon) {
    auto *device = directXCommon_->GetDevice(Passkey<GraphicsEngine>{});
    auto &settings = GetEngineSettings().rendering;
    pipelineManager_ = std::make_unique<PipelineManager>(Passkey<GraphicsEngine>{},
        device, settings.pipelineSettingsPath);
    Window::SetPipelineManager(Passkey<GraphicsEngine>{}, pipelineManager_.get());
    renderer_ = std::make_unique<Renderer>(Passkey<GraphicsEngine>{}, 1024, directXCommon_, pipelineManager_.get());
    Window::SetRenderer(Passkey<GraphicsEngine>{}, renderer_.get());
}

GraphicsEngine::~GraphicsEngine() {
    Window::SetPipelineManager(Passkey<GraphicsEngine>{}, nullptr);
    Window::SetRenderer(Passkey<GraphicsEngine>{}, nullptr);
}

void GraphicsEngine::RenderFrame(Passkey<GameEngine>) {
    // テスト用（Object2DBase / Object3DBase の vector に格納できるか）
    static std::vector<std::unique_ptr<Object3DBase>> testObjects3D;
    static std::vector<std::unique_ptr<Object2DBase>> testObjects2D;

    static bool initialized = false;

    auto mainWindow = Window::GetWindow("Main Window");
    auto overlayWindow = Window::GetWindow("Overlay Window");
    auto *targetWindow = overlayWindow ? overlayWindow : mainWindow;

    auto attachIfPossible3D = [&](Object3DBase *obj) {
        if (!obj || !targetWindow) return;
        obj->AttachToRenderer(targetWindow, "Object3D.Solid.BlendNormal");
    };
    auto attachIfPossible2D = [&](Object2DBase *obj) {
        if (!obj || !targetWindow) return;
        obj->AttachToRenderer(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal");
    };

#if defined(USE_IMGUI)
    static float updateMs = 0.0f;
    static float passRegisterMs = 0.0f;
    static float renderFrameMs = 0.0f;
    static float totalMs = 0.0f;

    static float fps = 0.0f;
    static float frameMs = 0.0f;

    static int spawn3DCount = 10;
    static int spawn2DCount = 10;

    static int profilingSampleCount = 60;

    static RollingAverage avgUpdateMs(static_cast<std::size_t>(profilingSampleCount));
    static RollingAverage avgPassRegisterMs(static_cast<std::size_t>(profilingSampleCount));
    static RollingAverage avgRenderFrameMs(static_cast<std::size_t>(profilingSampleCount));
    static RollingAverage avgTotalMs(static_cast<std::size_t>(profilingSampleCount));
    static RollingAverage avgFps(static_cast<std::size_t>(profilingSampleCount));
#endif

#if defined(USE_IMGUI)
    const auto frameBeginTp = std::chrono::high_resolution_clock::now();
#endif

    //--------- 初期化 ---------//
    if (!initialized) {
        //==================================================
        // 3Dオブジェクトの初期化
        //==================================================
        testObjects3D.clear();
        testObjects3D.reserve(2 + (kEnableInstancingTest ? kInstancingTestCount3D : 0));

        // Camera3D
        testObjects3D.emplace_back(std::make_unique<Camera3D>());
        if (auto *camera = static_cast<Camera3D *>(testObjects3D.back().get())) {
            if (auto *transformComp = camera->GetComponent3D<Transform3D>()) {
                transformComp->SetTranslate(Vector3(0.0f, 0.0f, -10.0f));
            }
        }
        attachIfPossible3D(static_cast<Object3DBase *>(testObjects3D.back().get()));

        // DirectionalLight
        testObjects3D.emplace_back(std::make_unique<DirectionalLight>());
        if (auto *light = static_cast<DirectionalLight *>(testObjects3D.back().get())) {
            light->SetEnabled(true);
            light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            light->SetDirection(Vector3(0.3f, -1.0f, 0.2f));
            light->SetIntensity(1.0f);
        }
        attachIfPossible3D(static_cast<Object3DBase *>(testObjects3D.back().get()));

        // インスタンシング用の三角形
        {
            const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount3D : 0;
            Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 3DCount=" + std::to_string(instanceCount),
                LogSeverity::Info);
            for (std::uint32_t i = 0; i < (kEnableInstancingTest ? kInstancingTestCount3D : 0); ++i) {
                auto obj = std::make_unique<Triangle3D>();
                obj->SetName(std::string("InstancingTriangle3D_") + std::to_string(i));

                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    const float x = (static_cast<float>(i % 16) - 8.0f) * 0.4f;
                    const float y = (static_cast<float>(i / 16) - 2.0f) * 0.4f;
                    tr->SetTranslate(Vector3(x, y, 0.0f));
                }
                attachIfPossible3D(obj.get());
                testObjects3D.emplace_back(std::move(obj));
            }
        }

        //==================================================
        // 2Dオブジェクトの初期化
        //==================================================
        testObjects2D.clear();
        testObjects2D.reserve(1 + (kEnableInstancingTest ? kInstancingTestCount2D : 0));

        // Camera2D
        testObjects2D.emplace_back(std::make_unique<Camera2D>());
        attachIfPossible2D(static_cast<Object2DBase *>(testObjects2D.back().get()));

        // インスタンシング用の三角形
        {
            const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount2D : 0;
            Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 2DCount=" + std::to_string(instanceCount),
                LogSeverity::Info);
            for (std::uint32_t i = 0; i < (kEnableInstancingTest ? kInstancingTestCount2D : 0); ++i) {
                auto obj = std::make_unique<Triangle2D>();
                obj->SetName(std::string("InstancingTriangle2D_") + std::to_string(i));
                if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                    const float x = 50.0f + static_cast<float>(i % 16) * 30.0f;
                    const float y = 200.0f + static_cast<float>(i / 16) * 30.0f;
                    tr->SetTranslate(Vector2(x, y));
                    tr->SetScale(Vector2(20.0f, 20.0f));
                }
                attachIfPossible2D(obj.get());
                testObjects2D.emplace_back(std::move(obj));
            }
        }

        initialized = true;
    }

#if defined(USE_IMGUI)
    const auto updateBeginTp = std::chrono::high_resolution_clock::now();
#endif

    //--------- テスト用の更新処理 ---------//

    // 3D: Camera / Light 以外は回転
    for (auto &obj : testObjects3D) {
        if (!obj) continue;
        if (obj->GetName() == "Camera3D") continue;
        if (obj->GetName() == "DirectionalLight") continue;

        if (auto *transformComp = obj->GetComponent3D<Transform3D>()) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }

#if defined(USE_IMGUI)
    const auto updateEndTp = std::chrono::high_resolution_clock::now();
    updateMs = std::chrono::duration<float, std::milli>(updateEndTp - updateBeginTp).count();
    const auto passBeginTp = updateEndTp;

    if (ImGui::Begin("RenderPass Spawn/Despawn Test")) {
        ImGui::Text("3D objects: %d", static_cast<int>(testObjects3D.size()));
        ImGui::SliderInt("Spawn 3D", &spawn3DCount, 1, 500);
        if (ImGui::Button("Add 3D Triangles")) {
            const size_t base = testObjects3D.size();
            for (int i = 0; i < spawn3DCount; ++i) {
                auto obj = std::make_unique<Triangle3D>();
                obj->SetName(std::string("SpawnedTriangle3D_") + std::to_string(base + static_cast<size_t>(i)));
                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    const float x = (static_cast<float>((base + static_cast<size_t>(i)) % 16) - 8.0f) * 0.4f;
                    const float y = (static_cast<float>((base + static_cast<size_t>(i)) / 16) - 2.0f) * 0.4f;
                    tr->SetTranslate(Vector3(x, y, 0.0f));
                }
                attachIfPossible3D(obj.get());
                testObjects3D.emplace_back(std::move(obj));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove 3D (last)")) {
            // Keep camera/light
            if (testObjects3D.size() > 2) {
                testObjects3D.pop_back();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove 3D x10")) {
            for (int i = 0; i < 10; ++i) {
                if (testObjects3D.size() <= 2) break;
                testObjects3D.pop_back();
            }
        }

        ImGui::Separator();
        ImGui::Text("2D objects: %d", static_cast<int>(testObjects2D.size()));
        ImGui::SliderInt("Spawn 2D", &spawn2DCount, 1, 500);
        if (ImGui::Button("Add 2D Triangles")) {
            const size_t base = testObjects2D.size();
            for (int i = 0; i < spawn2DCount; ++i) {
                auto obj = std::make_unique<Triangle2D>();
                obj->SetName(std::string("SpawnedTriangle2D_") + std::to_string(base + static_cast<size_t>(i)));
                if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                    const float x = 50.0f + static_cast<float>((base + static_cast<size_t>(i)) % 16) * 30.0f;
                    const float y = 200.0f + static_cast<float>((base + static_cast<size_t>(i)) / 16) * 30.0f;
                    tr->SetTranslate(Vector2(x, y));
                    tr->SetScale(Vector2(20.0f, 20.0f));
                }
                attachIfPossible2D(obj.get());
                testObjects2D.emplace_back(std::move(obj));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove 2D (last)")) {
            // Keep camera
            if (testObjects2D.size() > 1) {
                testObjects2D.pop_back();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove 2D x10")) {
            for (int i = 0; i < 10; ++i) {
                if (testObjects2D.size() <= 1) break;
                testObjects2D.pop_back();
            }
        }
    }
    ImGui::End();
#endif

#if defined(USE_IMGUI)
    const auto passEndTp = std::chrono::high_resolution_clock::now();
    passRegisterMs = std::chrono::duration<float, std::milli>(passEndTp - passBeginTp).count();

    const auto renderBeginTp = passEndTp;
#endif

    renderer_->RenderFrame({});

#if defined(USE_IMGUI)
    const auto renderEndTp = std::chrono::high_resolution_clock::now();
    renderFrameMs = std::chrono::duration<float, std::milli>(renderEndTp - renderBeginTp).count();
    totalMs = std::chrono::duration<float, std::milli>(renderEndTp - frameBeginTp).count();

    {
        const float dtMs = totalMs;
        frameMs = dtMs;
        fps = (dtMs > 0.0f) ? (1000.0f / dtMs) : 0.0f;
    }

    // Apply sample count change from ImGui (deferred until end of frame to avoid mid-frame resets)
    {
        const std::size_t cap = static_cast<std::size_t>(std::max(1, profilingSampleCount));
        avgUpdateMs.SetCapacity(cap);
        avgPassRegisterMs.SetCapacity(cap);
        avgRenderFrameMs.SetCapacity(cap);
        avgTotalMs.SetCapacity(cap);
        avgFps.SetCapacity(cap);

        avgUpdateMs.Add(static_cast<double>(updateMs));
        avgPassRegisterMs.Add(static_cast<double>(passRegisterMs));
        avgRenderFrameMs.Add(static_cast<double>(renderFrameMs));
        avgTotalMs.Add(static_cast<double>(totalMs));
        avgFps.Add(static_cast<double>(fps));
    }

    if (ImGui::Begin("GraphicsEngine Profiling")) {
        ImGui::Text("FPS: %.2f (%.2f ms)", fps, frameMs);
        ImGui::Text("Update: %.3f ms", updateMs);
        ImGui::Text("PassRegister: %.3f ms", passRegisterMs);
        ImGui::Text("Renderer::RenderFrame: %.3f ms", renderFrameMs);
        ImGui::Separator();
        ImGui::Text("Total: %.3f ms", totalMs);

        ImGui::Separator();
        ImGui::SliderInt("Profiling Sample Count", &profilingSampleCount, 1, 600);
        ImGui::Text("Averages (%zu samples)", avgTotalMs.GetCount());
        ImGui::Text("Avg FPS: %.2f", static_cast<float>(avgFps.GetAverage()));
        ImGui::Text("Avg Update: %.3f ms", static_cast<float>(avgUpdateMs.GetAverage()));
        ImGui::Text("Avg PassRegister: %.3f ms", static_cast<float>(avgPassRegisterMs.GetAverage()));
        ImGui::Text("Avg Renderer::RenderFrame: %.3f ms", static_cast<float>(avgRenderFrameMs.GetAverage()));
        ImGui::Text("Avg Total: %.3f ms", static_cast<float>(avgTotalMs.GetAverage()));
    }
    ImGui::End();
#endif
}

} // namespace KashipanEngine
