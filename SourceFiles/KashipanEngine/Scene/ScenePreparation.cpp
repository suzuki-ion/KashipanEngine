#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Scene/Components/InputDeviceSettings.h"
#if defined(USE_IMGUI)
#include "Scene/SceneEditor.h"
#include "Scene/SceneEditorContext.h"
#endif
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace KashipanEngine {
struct Scene::Preparation {
    JSON data;
    std::vector<std::pair<ISceneComponent*,JSON>> components;
    std::vector<EmptyObject*> objects;
    size_t cursor=0, completed=0, total=1;
    enum class Phase { Header, CreateComponents, LoadComponents, CreateObjects, LoadObjects, Ready, Failed } phase=Phase::Header;
    std::string error;
};

void Scene::BeginPreparation(Passkey<SceneManager>, JSON data) {
    if (!objects_.empty() || !components_.empty() || preparation_) throw std::logic_error("Scene preparation requires an empty scene");
    preparation_=std::make_shared<Preparation>();
    preparation_->data=std::move(data); isPreparing_=true;
    auto &j=preparation_->data;
    if(!j.is_object() || (j.contains("sceneObjects") && !j["sceneObjects"].is_array()) ||
        (j.contains("sceneComponents") && !j["sceneComponents"].is_array())) {
        preparation_->error="Invalid scene document"; preparation_->phase=Preparation::Phase::Failed; return;
    }
    if(!j.contains("sceneObjects"))j["sceneObjects"]=JSON::array();
    if(!j.contains("sceneComponents"))j["sceneComponents"]=JSON::array();
#if !defined(USE_IMGUI)
    // Editor-only component factories do not exist in Release. Remove the whole
    // editor-only subtree before validating/constructing any of its components.
    std::unordered_map<std::string,std::vector<std::string>> children;
    std::vector<std::string> pending;
    std::unordered_set<std::string> excluded;
    for(const auto &object:j["sceneObjects"]) {
        const auto id=object.value("objectID",std::string{});
        if(object.value("editorOnly",false) && !id.empty())pending.push_back(id);
        for(const auto &component:object.value("components",JSON::array())) {
            if(component.value("type",std::string{})!="Transform")continue;
            const auto transformData=component.value("data",JSON::object()).value("customData",JSON::object());
            const auto parent=transformData.value("parent",std::string{});
            if(!parent.empty())children[parent].push_back(id);
        }
    }
    while(!pending.empty()) {
        auto id=std::move(pending.back());pending.pop_back();
        if(!excluded.insert(id).second)continue;
        if(auto it=children.find(id);it!=children.end())pending.insert(pending.end(),it->second.begin(),it->second.end());
    }
    auto &objects=j["sceneObjects"];
    objects.erase(std::remove_if(objects.begin(),objects.end(),[&](const JSON &object) {
        const auto id=object.value("objectID",std::string{});
        return object.value("editorOnly",false) || (!id.empty() && excluded.contains(id));
    }),objects.end());
#endif
    std::unordered_set<std::string> ids;
    for(const auto &object:j["sceneObjects"]) {
        const auto id=object.value("objectID",std::string{});
        if(!UUID128(id).IsValid() || !ids.insert(id).second) {
            preparation_->error="Scene object UUID is missing, invalid or duplicated";
            preparation_->phase=Preparation::Phase::Failed; return;
        }
    }
    preparation_->total=1+2*j["sceneObjects"].size()+2*j["sceneComponents"].size();
}

bool Scene::AdvancePreparation(Passkey<SceneManager>,double budgetMilliseconds) {
    if(!preparation_)return false;
    auto &p=*preparation_; using Phase=Preparation::Phase;
    const auto start=std::chrono::steady_clock::now();
    try {
        do {
            switch(p.phase) {
            case Phase::Header: {
                JSON header={{"sceneName",p.data.value("sceneName",name_)},{"sceneID",p.data.value("sceneID",sceneID_.ToString())},
                    {"sceneVariables",p.data.value("sceneVariables",JSON::array())}};
                if(!LoadFromJSON(header))throw std::runtime_error("Invalid scene metadata");
                p.phase=Phase::CreateComponents; ++p.completed; break;
            }
            case Phase::CreateComponents: {
                const auto &list=p.data["sceneComponents"];
                if(p.cursor==list.size()) { p.phase=Phase::LoadComponents; p.cursor=0; continue; }
                const auto &c=list[p.cursor++]; auto component=CreateSceneComponentByType(c.value("type",std::string{}));
                if(!component)throw std::runtime_error("Unknown scene component: "+c.value("type",std::string{}));
                const auto data=c.value("data",JSON::object()); component->SetActive(data.value("isActive",true));
                auto *added=AddComponent(std::move(component)); if(!added)throw std::runtime_error("Could not add scene component");
                p.components.emplace_back(added,data); ++p.completed; break;
            }
            case Phase::LoadComponents:
                if(p.cursor==p.components.size()) { p.phase=Phase::CreateObjects; p.cursor=0; continue; }
                if(!p.components[p.cursor].first->LoadFromJsonInterface(Passkey<Scene>{},p.components[p.cursor].second))throw std::runtime_error("Could not load scene component");
                ++p.cursor; ++p.completed; break;
            case Phase::CreateObjects: {
                const auto &list=p.data["sceneObjects"];
                if(p.cursor==list.size()) { p.phase=Phase::LoadObjects; p.cursor=0; continue; }
                const auto &o=list[p.cursor++];
                if(!o.is_object())throw std::runtime_error("Invalid scene object");
                auto *object=CreateEmptyObject(o.value("name",std::string("Empty Object")),UUID128(o.value("objectID",std::string{})));
                if(!object)throw std::runtime_error("Could not create scene object");
                p.objects.push_back(object); ++p.completed; break;
            }
            case Phase::LoadObjects: {
                if(p.cursor==p.objects.size()) {
#if !defined(USE_IMGUI)
                    DeleteEditorOnlyObjects();
#endif
                    p.phase=Phase::Ready; return true;
                }
                const auto &data=p.data["sceneObjects"][p.cursor];
                // Unknown types must fail rather than silently claiming the target is ready.
                for(const auto &c:data.value("components",JSON::array()))if(!CreateObjectComponentByType(c.value("type",std::string{})))
                    throw std::runtime_error("Unknown object component: "+c.value("type",std::string{}));
                auto *object=p.objects[p.cursor++];
                if(!object->LoadFromJson(Passkey<Scene>{},data))throw std::runtime_error("Could not load scene object");
                for(auto *script:object->GetComponents<ScriptComponent>())
                    if(script->IsActive() && !script->GetScriptPath().empty() && !script->GetLastError().empty())throw std::runtime_error(script->GetLastError());
                ++p.completed; break;
            }
            case Phase::Ready:return true;
            case Phase::Failed:return false;
            }
        } while(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<budgetMilliseconds);
    }catch(const std::exception &e) { p.error=e.what(); p.phase=Phase::Failed; }
    return p.phase==Phase::Ready;
}
float Scene::GetPreparationProgress() const {
    return preparation_ ? std::min(1.f,static_cast<float>(preparation_->completed)/static_cast<float>(preparation_->total)) : 0;
}
const std::string &Scene::GetPreparationError() const { static const std::string empty; return preparation_ ? preparation_->error : empty; }

JSON Scene::ActivatePreparation(Passkey<SceneManager>) {
    if(!preparation_ || preparation_->phase!=Preparation::Phase::Ready)throw std::logic_error("Scene is not ready");
    auto snapshot=std::move(preparation_->data); preparation_.reset(); isPreparing_=false;
#if defined(USE_IMGUI)
    sceneEditorContext_=std::make_unique<SceneEditorContext>(Passkey<Scene>{},this);
    sceneEditor_=std::make_unique<SceneEditor>(Passkey<Scene>{},sceneEditorContext_.get());
#endif
    if(auto *input=GetComponent<InputDeviceSettings>(); input && input->IsActive())input->Apply();
    // Snapshot IDs: user constructors/Awake may delete or append objects and components.
    std::vector<ComponentRef> windows,scripts,buffers;
    for(auto *object:objects_)for(const auto &[component,id]:object->GetObjectContext()->GetAllComponents()) {
        (void)id;
        if(dynamic_cast<IWindowObjectComponent*>(component))windows.push_back(component->GetComponentRef());
        if(dynamic_cast<ScriptComponent*>(component))scripts.push_back(component->GetComponentRef());
        if(dynamic_cast<ScreenBufferObject*>(component))buffers.push_back(component->GetComponentRef());
    }
    for(const auto &ref:buffers)if(auto *b=dynamic_cast<ScreenBufferObject*>(sceneContext_->ResolveComponent(ref)); b && b->IsActive())b->ActivatePreparedBuffer(Passkey<Scene>{});
    for(const auto &ref:windows)if(auto *w=dynamic_cast<IWindowObjectComponent*>(sceneContext_->ResolveComponent(ref)); w && w->IsActive())w->ActivatePreparedWindow(Passkey<Scene>{});
    const bool previous=BeginDeferredDestroy();
    try {
        for(const auto &ref:scripts)if(auto *s=dynamic_cast<ScriptComponent*>(sceneContext_->ResolveComponent(ref)); s && s->IsActive())s->ActivatePreparedScript(Passkey<Scene>{});
    }catch(...) { EndDeferredDestroy(previous); throw; }
    EndDeferredDestroy(previous);
    return snapshot;
}
}
