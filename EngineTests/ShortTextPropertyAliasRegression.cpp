#include <bit>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <set>
#include <stdexcept>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/PropertyMotion.h"
using namespace KashipanEngine;
static void Check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
int Run(){try{
    size_t members=0;
    std::set<std::string> objects,scenes;
    for(const auto &entry:ShortText::PropertyAliasCatalog()) {
        auto &names=entry.scene?scenes:objects;Check(names.insert(entry.component).second,"duplicate type "+entry.component);
        Check(entry.aliases.size()>=2,"multiple component names "+entry.component);
        if(entry.scene)continue;
        auto component=CreateObjectComponentByType(entry.component);Check(component!=nullptr,"registered type "+entry.component);
        std::set<std::string> listed;
        for(const auto &item:entry.members){
            Check(component->GetMemberVariable(item.member)!=nullptr,"alias targets nonexistent member "+entry.component+"."+item.member);
            Check(item.aliases.size()>=2,"multiple member names "+item.member);listed.insert(item.member);++members;
        }
        for(const auto &[name,variable]:component->GetAllMemberVariables()) {
            (void)variable;Check(listed.contains(name),"missing member aliases "+entry.component+"."+name);
        }
    }
    for(const auto &name:GetRegisteredObjectComponentTypes())Check(objects.contains(name),"missing object type "+name);
    for(const auto &name:GetRegisteredSceneComponentTypes())Check(scenes.contains(name),"missing scene type "+name);
    Scene scene(std::string("AliasRegression"));auto *sc=scene.GetSceneContext();
    SceneEditorContext editor(std::bit_cast<Passkey<Scene>>(std::uint8_t{}),&scene);
    auto *target=sc->CreateEmptyObject("target");
    const std::string text="Lightの色を(0.5f, 0.5f, 1.0f, 1.0f)にして";
    auto parsed=ParseShortTextPropertyRequest(text,target);Check(parsed.success,parsed.message);
    auto command=MakeShortTextPropertyCommand(target,parsed.request,text);Check(command!=nullptr,"tuple plan");
    Check(!target->GetComponent<Light>(),"planning is side-effect free");
    Check(command->Execute(&editor),"tuple apply");
    auto *light=target->GetComponent<Light>();Check(light!=nullptr,"missing component added");
    Check(light->GetColor()==Vector4(0.5f,0.5f,1,1),"RGBA applied to real Light");
    Check(command->Undo(&editor)&&!target->GetComponent<Light>(),"undo removes generated component");
    Check(command->Execute(&editor),"redo");
    light=target->GetComponent<Light>();light->SetColor(Vector4(0.2f,0.3f,0.4f,0.5f));
    auto update=MakeShortTextPropertyCommand(target,parsed.request,text);
    Check(update&&update->Execute(&editor)&&target->GetComponents<Light>().size()==1,"existing component is reused");
    Check(update->Undo(&editor)&&light->GetColor()==Vector4(0.2f,0.3f,0.4f,0.5f),"RGBA undo restores all original channels");
    Check(update->Execute(&editor),"RGBA update redo");
    const auto snapshot=scene.SaveToJSON();Scene restored(JSON::parse(snapshot.dump()));
    Check(restored.GetSceneContext()->GetSceneObject(target->GetObjectID())->GetComponent<Light>()->GetColor()==Vector4(0.5f,0.5f,1,1),"scene roundtrip");
    const auto before=scene.SaveToJSON().dump();
    parsed=ParseShortTextPropertyRequest("Lightの色を(1,2,3)にする",target);
    Check(!parsed.success && scene.SaveToJSON().dump()==before,"wrong arity causes no changes");
    auto *transform=target->GetComponent<Transform>();transform->SetTranslate({7,8,9});(void)transform->GetWorldMatrix();
    parsed=ParseShortTextPropertyRequest("Transformの移動を(1,2,3)にする",target);
    Check(parsed.success,"Vector3 parse");auto move=MakeShortTextPropertyCommand(target,parsed.request,"Transformの移動を(1,2,3)にする");
    Check(move&&move->Execute(&editor)&&transform->GetTranslate()==Vector3(1,2,3),"Vector3 setter");
    Check(transform->GetWorldPosition()==Vector3(1,2,3),"setter invalidates the cached world transform");
    Check(move->Undo(&editor)&&transform->GetTranslate()==Vector3(7,8,9),"whole-vector undo");
    // A fixed tuple replaces all managed tracks of that member, not just channel zero.
    for(const auto &sentence:std::vector<std::string>{"Transformの移動X軸を0から1の間で往復","Transformの移動Y軸を0から2の間で往復"}) {
        parsed=ParseShortTextPropertyRequest(sentence,target);Check(parsed.success,"motion parse");
        auto motion=MakeShortTextPropertyCommand(target,parsed.request,sentence);Check(motion&&motion->Execute(&editor),"motion add");
    }
    Check(target->GetComponents<PropertyMotion>().size()==2,"two managed channels before tuple");
    parsed=ParseShortTextPropertyRequest("Transformの位置を(4,5,6)にする",target);
    auto fixed=MakeShortTextPropertyCommand(target,parsed.request,"Transformの位置を(4,5,6)にする");
    Check(fixed&&fixed->Execute(&editor)&&target->GetComponents<PropertyMotion>().empty(),"whole-member setting removes both managed motions");
    Check(fixed->Undo(&editor)&&target->GetComponents<PropertyMotion>().size()==2&&transform->GetTranslate()==Vector3(7,8,9),"undo restores both motions and vector");
    std::printf("Property alias registry and tuple regression passed: %zu object / %zu scene / %zu members\n",objects.size(),scenes.size(),members);return 0;
}catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
int main(){const auto original=std::filesystem::current_path();const auto folder=std::filesystem::temp_directory_path()/("PropertyAliases-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(folder);std::filesystem::current_path(folder);const int code=Run();std::filesystem::current_path(original);std::filesystem::remove_all(folder);return code;}
