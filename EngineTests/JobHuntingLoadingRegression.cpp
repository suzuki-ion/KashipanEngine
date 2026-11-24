#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneFileIO.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Transform.h"
#include "Utilities/Conversion/ConvertString.h"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace KashipanEngine;
namespace {
const auto key=std::bit_cast<Passkey<GameEngine>>(std::uint8_t{});
int checks=0;
void Check(bool v,const std::string &message){++checks;if(!v)throw std::runtime_error(message);}
JSON Component(const char *type,JSON data){return JSON{{"type",type},{"data",{{"isActive",true},{"customData",data}}}};}
JSON Object(const char *name){return JSON{{"objectID",UUID128(true).ToString()},{"name",name},{"components",JSON::array({Component("Transform",{{"translate",{{"x",640},{"y",360},{"z",0}}},{"scale",{{"x",1280},{"y",720},{"z",1}}},{"rotate",{{"x",0},{"y",0},{"z",0},{"w",1}}}})})}};}
Scene *Current(SceneManager &manager){return const_cast<Scene*>(manager.GetCurrentScene());}
}
int main(){
    const auto root=std::filesystem::current_path();
    const auto scripts=root/"GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown";
    const auto output=root/"Generated/JobHuntingLoading";std::filesystem::create_directories(output);
    try{
        Scene compiler(std::string("LoadingScriptChecks"));
        for(const auto *name:{"TitleController","CityController","StageSelectController","ExpeditionController","LoadingController"}){
            auto *object=compiler.GetSceneContext()->CreateEmptyObject(name);auto *script=object->AddComponent<ScriptComponent>();
            script->SetScriptPath(PathToUtf8String(scripts/(std::string(name)+".as")));
            Check(script->Reload(),std::string(name)+": "+script->GetLastError());
        }
        std::ofstream probe(output/"Route.as");
        probe<<"#include "<<JSON(PathToUtf8String(scripts/"TownState.as")).dump()<<"\n#include "<<JSON(PathToUtf8String(scripts/"TownTransition.as")).dump()<<R"(
class RouteProbe : ScriptComponentBehavior {
 [SerializeField] string target;
 TownTransition@ transition=TownTransition();
 void Start(){transition.Initialize(GetScene());GetScene().SetNextSceneName(target);if(target=="StageSelectScene")transition.Begin();else transition.Begin(target);}
 void Update(){transition.Update(.05f);}
})";probe.close();
        const auto actual=LoadSceneFromPath(PathToUtf8String(root/"GameProjects/JobHuntingGame/Assets/Scenes/LoadingScene.scene"));
        Check(actual["sceneObjects"].size()==17,"minimal loading scene has 17 objects");
        for(const auto *target:{"CityScene","ExpeditionScene","StageSelectScene"}){
            SceneManager manager(key);auto loading=actual;
            for(auto &object:loading["sceneObjects"])for(auto &component:object["components"]){
                if(component["type"]=="NormalWindowObject")component["data"]["isActive"]=false; // ネイティブウィンドウや GPU を使わないヘッドレステスト。
                if(component["type"]=="ScriptComponent"){
                    auto &d=component["data"]["customData"];d["scriptPath"]=PathToUtf8String(scripts/"LoadingController.as");
                    d["fields"]["minimumDisplaySeconds"]=0;d["fields"]["iconFadeSeconds"]=0;
                }
            }
            manager.RegisterScene("LoadingScene",loading);
            for(const auto *name:{"CityScene","ExpeditionScene","StageSelectScene"})manager.RegisterScene(name,JSON{{"sceneName",name}});
            auto fade=Object("TownFadeTemplate");fade["isActive"]=false;fade["components"].push_back(Component("SpriteRenderer",JSON::object()));
            auto view=Object("TownDisplayViewport");auto controller=Object("Route");
            controller["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(output/"Route.as")},{"fields",{{"target",target}}}}));
            manager.RegisterScene("Source",JSON{{"sceneName","Source"},{"sceneObjects",JSON::array({view,fade,controller})}});
            Check(manager.ChangeScene("Source")&&manager.CommitPendingSceneChange(key),"source scene starts");
            Current(manager)->ContinuePlayAfterSceneChange(std::bit_cast<Passkey<SceneManager>>(std::uint8_t{}));
            bool visited=false,arrived=false;
            for(int frame=0;frame<100;++frame){
                manager.Update(key);manager.CommitPendingSceneChange(key);
                Check(manager.GetSceneLoadError().empty(),manager.GetSceneLoadError());
                if(Current(manager)->GetName()=="LoadingScene"&&!visited){
                    visited=true;Check(manager.GetSceneLoadTarget()==target,"loading target preserved");
                    auto *spinner=Current(manager)->GetSceneContext()->GetSceneObject("LoadingSpinner");
                    auto *animator=spinner->GetComponent<KeyFrameAnimator>();Check(animator&&animator->SetCurrentTime("LoadingSpin",.25f),"spinner uses real KeyFrameAnimator");
                    Check(std::abs(spinner->GetComponent<Transform>()->GetRotate().z+1.5707963f)<.0001f,"spinner makes a clockwise quarter turn");
                    auto *spoke=Current(manager)->GetSceneContext()->GetSceneObject("LoadingSpoke0");const auto world=spoke->GetComponent<Transform>()->GetWorldMatrix();
                    Check(std::abs(world.m[3][0]-1232)<.001f&&std::abs(world.m[3][1]-32)<.001f,"spokes rotate around the bottom-right anchor");
                }
                if(Current(manager)->GetName()==target){arrived=true;break;}
            }
            Check(arrived,std::string("arrived: ")+target);
            Check(visited==(std::string(target)!="StageSelectScene"),"only town and expedition use LoadingScene");
        }
        std::cout<<checks<<" JobHuntingGame loading checks passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
