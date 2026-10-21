#include "Scene/Scene.h"
#ifdef USE_IMGUI
#include "Core/DirectXCommon.h"
#include "Graphics/GraphicsEngine.h"
#include "Scene/RenderTargetCarryOverRegistry.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include <algorithm>
namespace KashipanEngine {
bool Scene::EditorReplaceScene(const JSON &json) {
    if (!json.is_object() || !json.contains("sceneName")) return false;
    if (sDirectXCommon_) sDirectXCommon_->WaitForGPUIdle(Passkey<Scene>{});
    RenderTargetCarryOverRegistry::BeginSceneSwitch(Passkey<Scene>{});
    ClearSceneObjects(); ClearSceneComponents();
    if (sGraphicsEngine_) sGraphicsEngine_->ReleaseRendererResources(Passkey<Scene>{});
    bool loaded = false;
    try { loaded = LoadFromJSON(json); } catch (...) { }
    RenderTargetCarryOverRegistry::EndSceneSwitch(Passkey<Scene>{});
    return loaded;
}
JSON Scene::EditorSaveSceneComponent(size_t id) const {
    for (const auto &[component, addedID] : components_) if (component && addedID == id)
        return JSON{{"type", component->GetComponentType()}, {"data", component->SaveToJsonInterface(Passkey<Scene>{})}};
    return JSON();
}
bool Scene::EditorLoadSceneComponent(size_t id, const JSON &json) {
    for (const auto &[component, addedID] : components_) if (component && addedID == id) {
        if (json.value("type", std::string{}) != component->GetComponentType()) return false;
        auto data = json.at("data"); const bool active = data.value("isActive", true);
        data["isActive"] = component->IsActive();
        if (!component->LoadFromJsonInterface(Passkey<Scene>{}, data)) return false;
        component->SetActive(active); return true;
    }
    return false;
}
bool Scene::EditorRestoreSceneComponent(size_t id, const JSON &json) {
    if (id >= MAXSIZE_T - 1 || !EditorSaveSceneComponent(id).is_null()) return false;
    auto created = CreateSceneComponentByType(json.at("type").get<std::string>());
    if (!created) return false; created->SetActive(false);
    auto *component = AddComponent(std::move(created));
    if (!component) return false;
    for (auto &[current, addedID] : components_) if (current.get() == component) { addedID = id; break; }
    nextAddedComponentID_ = (std::max)(nextAddedComponentID_, id + 1);
    try { if (EditorLoadSceneComponent(id, json)) return true; } catch (...) { }
    RemoveComponent(component); return false;
}
}
#endif
