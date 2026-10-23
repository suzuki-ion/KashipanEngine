// Headless lifecycle/update/cache regressions using the real scene and component pools.
#include <bit>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/Renderer/RendererInternal.h"
using namespace KashipanEngine;
namespace KashipanEngine {
class OptimizationProbe final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(OptimizationProbe, 0xFF,)
    std::unique_ptr<IObjectComponent> Clone() const override { return std::make_unique<OptimizationProbe>(); }
    int calls = 0, marker = 0;
    std::vector<int> *order = nullptr;
    std::function<void()> action;
protected:
    void Update() override { ++calls; if (order) order->push_back(marker); if (action) action(); }
};
REGISTER_COMPONENT_OBJECT(OptimizationProbe)
class OptimizationLifecycleOnly final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(OptimizationLifecycleOnly, 1,)
    std::unique_ptr<IObjectComponent> Clone() const override { return std::make_unique<OptimizationLifecycleOnly>(); }
    int initialized = 0, finalized = 0;
protected:
    void Initialize() override { ++initialized; }
    void Finalize() override { ++finalized; }
};
REGISTER_COMPONENT_OBJECT(OptimizationLifecycleOnly)
}
void Check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
class HeadlessLightTarget final : public IRenderTarget {
public:
    RenderTargetKind GetRenderTargetKind() const noexcept override { return RenderTargetKind::ScreenBuffer; }
    std::string GetRenderTargetName() const override { return "HeadlessLightTarget"; }
    std::uint32_t GetRenderTargetWidth() const noexcept override { return 1; }
    std::uint32_t GetRenderTargetHeight() const noexcept override { return 1; }
    bool IsRenderTargetAvailable() const noexcept override { return true; }
    void BeginDraw() override {} void EndDraw() override {}
    ID3D12GraphicsCommandList *GetCommandList() const override { return nullptr; }
};
int main() {
 try {
    Scene scene(std::string("RendererOptimizationRegression")); auto *sc = scene.GetSceneContext();
    const auto update = std::bit_cast<Passkey<Scene>>(std::uint8_t{});
    const auto componentKey = std::bit_cast<Passkey<EmptyObject>>(std::uint8_t{});
    auto *parent=sc->CreateEmptyObject("Parent"), *child=sc->CreateEmptyObject("Child"), *other=sc->CreateEmptyObject("Other");
    child->GetComponent<Transform>()->SetParentObject(parent);
    for (int i=0;i<10000;++i) Check(child->IsActive(), "cached active child");
    parent->SetActive(false);Check(!child->IsActive(), "parent invalidates active cache");
    auto *grandchild=sc->CreateEmptyObject("Grandchild");grandchild->GetComponent<Transform>()->SetParentObject(child);
    Check(!grandchild->IsActive(), "new descendant inherits inactive ancestor");
    child->GetComponent<Transform>()->SetParentObject(other);
    Check(child->IsActive()&&grandchild->IsActive(), "reparent invalidates descendant caches");
    Check(!other->GetComponent<Transform>()->SetParentObject(grandchild), "hierarchy cycle rejected");
    other->SetActive(false); Check(!grandchild->IsActive(), "new ancestor deactivation");
    child->GetComponent<Transform>()->SetParentObject(nullptr);Check(grandchild->IsActive(), "detach invalidates descendant caches");
    child->SetActive(false); child->GetComponent<Transform>()->SetParentObject(parent);parent->SetActive(true);
    Check(!child->IsActive()&&!grandchild->IsActive(), "local inactive state survives ancestor activation");
    child->SetActive(true); Check(grandchild->IsActive(), "local reactivation");

    auto *owner=sc->CreateEmptyObject("Updates");auto *a=owner->AddComponent<OptimizationProbe>();auto *b=owner->AddComponent<OptimizationProbe>();
    auto *lifecycle=owner->AddComponent<OptimizationLifecycleOnly>();
    Check(!owner->GetComponent<Transform>()->HasFrameUpdate(componentKey)&&!lifecycle->HasFrameUpdate(componentKey)&&a->HasFrameUpdate(componentKey), "detect overridden updates without skipping lifecycle");
    Check(lifecycle->initialized==1, "non-updating component is initialized");
    std::vector<int> order;a->marker=1;b->marker=2;a->order=&order;b->order=&order;a->SetUpdatePriority(2);b->SetUpdatePriority(1);
    owner->UpdateInterface(update);Check(order==std::vector<int>({2,1}), "initial priority order");order.clear();
    owner->UpdateInterface(update);Check(order==std::vector<int>({2,1}), "cached update list");order.clear();
    a->SetUpdatePriority(0);owner->UpdateInterface(update);Check(order==std::vector<int>({1,2}), "priority changes invalidate cached list");order.clear();
    b->SetActive(false);owner->UpdateInterface(update);Check(order==std::vector<int>({1}), "component deactivation invalidates list");order.clear();b->SetActive(true);
    OptimizationProbe *replacement=nullptr;bool mutate=true;
    a->action=[&]{if(!mutate)return;mutate=false;owner->RemoveComponent(b);replacement=owner->AddComponent<OptimizationProbe>();replacement->marker=3;replacement->order=&order;};
    owner->UpdateInterface(update);Check(order==std::vector<int>({1})&&replacement->calls==0,"deleted/reused component skipped in immutable current-frame snapshot");order.clear();
    owner->UpdateInterface(update);Check(order==std::vector<int>({1,3}),"new component updates on next frame");order.clear();
    a->action=[&]{owner->SetActive(false);};owner->UpdateInterface(update);Check(order==std::vector<int>({1}),"owner disabled during update skips later components");
    Check(lifecycle->finalized==1,"non-updating component is finalized");a->action={};owner->SetActive(true);order.clear();owner->UpdateInterface(update);
    Check(order==std::vector<int>({1,3})&&lifecycle->initialized==2,"reactivation retains full lifecycle and cached updates");

    // The shared collection keeps the order of shadow sources identical to each GPU array.
    for (int kind=0;kind<8;++kind) {auto *o=sc->CreateEmptyObject("Light");auto *light=o->AddComponent<Light>();light->SetType(static_cast<Light::Type>(kind));o->AddComponent<LightRenderer>();}
    auto *registry=sc->GetComponent<SceneRenderer>();Check(registry!=nullptr,"light registry created");
    RendererInternal::FrameLights lights;
    HeadlessLightTarget target;
    RendererInternal::CollectLightsForTarget(registry,&target,"Object3D.Test",[](const LightRenderer*){return -1;},lights.pointLights,lights.spotLights,lights.directionalLights,lights.sphereLights,lights.discLights,lights.rectLights,lights.tubeLights,lights.boxLights,&lights.sources);
    for (const auto &sources:lights.sources)Check(sources.size()==1,"eight light types preserve shadow-source indexing");
    std::puts("PASS: active-state cache, reparent/detach, update priorities, pooled removal, immutable snapshots, lifecycle-only components, all light types");return 0;
 } catch(const std::exception &e){std::fprintf(stderr,"FAILED: %s\n",e.what());return 1;}
}
