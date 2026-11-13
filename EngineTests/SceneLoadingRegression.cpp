#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneFileIO.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Transform.h"
#include "Core/Window.h"
#include "Utilities/Conversion/ConvertString.h"
#include <angelscript.h>
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace KashipanEngine;
namespace {
const auto engineKey=std::bit_cast<Passkey<GameEngine>>(std::uint8_t{});
int checks=0;
void Check(bool v,const std::string &s) { ++checks; if(!v)throw std::runtime_error(s); }
JSON Component(const char *type,JSON data) { return JSON{{"type",type},{"data",{{"customData",data},{"isActive",true}}}}; }
JSON Object(const std::string &name) {
    return JSON{{"objectID",UUID128(true).ToString()},{"name",name},{"components",JSON::array({Component("Transform",{{"translate",{{"x",0},{"y",0},{"z",0}}},{"scale",{{"x",1},{"y",1},{"z",1}}},{"rotate",{{"x",0},{"y",0},{"z",0},{"w",1}}}})})}};
}
JSON SceneData(const char *name) { return JSON{{"sceneName",name},{"sceneID",UUID128(true).ToString()},{"sceneObjects",JSON::array()}}; }
int Counter(SceneManager &m,const char *key) { auto *v=m.GetGlobalSceneVariable(key); return v ? v->AnyCast<int>() : 0; }
Scene *Current(SceneManager &m) { return const_cast<Scene*>(m.GetCurrentScene()); }
void Wait(SceneManager &m) {
    for(int frame=0;frame<10000 && m.IsSceneLoading();++frame) {
        m.Update(engineKey);
        if(!m.GetSceneLoadError().empty())break;
        if(m.GetSceneLoadProgress()==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(m.IsSceneLoadReady(),"ready: "+m.GetSceneLoadError());
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const auto original=std::filesystem::current_path();
    const auto folder=original/"Generated/SceneLoading/Fixtures"; std::filesystem::create_directories(folder);
    std::filesystem::remove(folder/"Shutdown.json");
    std::filesystem::current_path(folder);
    try {
        std::ofstream("Loading.as") << R"(
class LoadingController : ScriptComponentBehavior {
    bool requested=false;
    void Update() {
        Scene@ s=GetScene(); int frames=0; s.GetGlobalVariable("loadingFrames",frames); s.SetGlobalVariable("loadingFrames",frames+1);
        bool activate=false; s.GetGlobalVariable("autoActivate",activate);
        if(activate && s.IsSceneLoadReady() && !requested) requested=s.ActivateLoadedScene();
    }
    void End() {
        Scene@ s=GetScene();
        if(s.HasGlobalVariable("endLoadTarget")) { string next; if(s.GetGlobalVariable("endLoadTarget",next)) s.BeginSceneLoad(next); }
    }
})";
        std::ofstream("Destination.as") << R"(
int Stamp(const string &in key) { Scene@ s=GetScene(); int n=0; if(!s.GetGlobalVariable(key,n)) n=0; s.SetGlobalVariable(key,n+1); return 123; }
int globalMarker=Stamp("globalInit");
class Destination : ScriptComponentBehavior {
    [SerializeField] float configured=2;
    Destination() { Stamp("ctor"); }
    void Awake() { Stamp("awake"); }
    void Start() { Stamp("start"); }
    void Update() { Stamp("destinationFrames"); }
    void End() { Stamp("end"); }
})";
        auto loading=SceneData("RegressionLoading"); auto controller=Object("Controller");
        controller["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(folder/"Loading.as")}})); loading["sceneObjects"].push_back(controller);
        auto target=SceneData("RegressionDestination");
        for(int i=0;i<24;++i)target["sceneObjects"].push_back(Object("Object"+std::to_string(i)));
        auto destination=Object("Destination"); destination["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(folder/"Destination.as")},{"fields",{{"configured",39.f}}}}));
        target["sceneObjects"].push_back(destination);
        auto editorObject=Object("Editor only script"); editorObject["editorOnly"]=true;
        editorObject["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(folder/"Destination.as")}})); target["sceneObjects"].push_back(editorObject);
#if !defined(USE_IMGUI)
        target["sceneObjects"].back()["components"].push_back(Component("EditorOnlyUnavailableComponent",JSON::object()));
        auto editorChild=Object("Editor only descendant");editorChild["components"][0]["data"]["customData"]["parent"]=editorObject["objectID"];
        editorChild["components"].push_back(Component("EditorOnlyUnavailableChildComponent",JSON::object()));target["sceneObjects"].push_back(editorChild);
#endif
        Check(SaveSceneToPath(target,PathToUtf8String(folder/"Target.scene")),"split scene fixture saved");
        {
        SceneManager manager(engineKey);
        manager.RegisterScene("Origin",SceneData("RegressionOrigin")); manager.RegisterScene("Loading",loading);
        manager.RegisterSceneFile("Target",PathToUtf8String(folder/"Target.scene"));
        manager.AddGlobalSceneVariable<int>("loadingFrames",0); manager.AddGlobalSceneVariable<bool>("autoActivate",false);
        Check(manager.ChangeScene("Origin") && manager.CommitPendingSceneChange(engineKey),"origin activated");
#if defined(USE_IMGUI)
        Current(manager)->PlayStart();
#endif
        Check(!manager.SetSceneLoadBudgetMilliseconds(0) && !manager.SetSceneLoadBudgetMilliseconds(std::numeric_limits<float>::quiet_NaN()),"invalid budgets rejected");
        Check(manager.SetSceneLoadBudgetMilliseconds(.000001f),"cooperative budget configured");
        Check(!manager.ChangeSceneWithLoading("Missing","Loading"),"unknown target rejected without changing scene");
        Check(manager.ChangeSceneWithLoading("Target","Loading"),"loading transition queued");
        Check(manager.IsSceneLoading(),"queued loading transition counts as loading"); manager.CancelSceneLoad();
        Check(!manager.HasPendingSceneChange() && !manager.IsSceneLoading() && Current(manager)->GetName()=="RegressionOrigin","cancel before loading scene entry retains origin");
        Check(manager.ChangeSceneWithLoading("Target","Loading"),"loading transition restarted");
        Check(Current(manager)->GetName()=="RegressionOrigin","transition deferred to frame end");
        Check(manager.CommitPendingSceneChange(engineKey),"loading scene committed");
        Check(Current(manager)->GetName()=="RegressionLoading" && manager.GetSceneLoadTarget()=="Target","loading scene runs while target reads");
        Check(!manager.ActivateLoadedScene(),"cannot activate incomplete scene");
        Wait(manager);
        Check(Counter(manager,"loadingFrames")>20,"loading script kept updating during construction");
        Check(Counter(manager,"globalInit")==0 && Counter(manager,"ctor")==0 && Counter(manager,"awake")==0 && Counter(manager,"start")==0,"target globals, constructor, Awake and Start deferred");
        Check(manager.GetSceneLoadProgress()==1,"ready progress is one");
        const auto loadingFrames=Counter(manager,"loadingFrames"); for(int i=0;i<5;++i)manager.Update(engineKey);
        Check(Current(manager)->GetName()=="RegressionLoading" && Counter(manager,"loadingFrames")==loadingFrames+5,"ready scene waits for explicit activation");
        auto *autoActivate=manager.GetGlobalSceneVariable("autoActivate"); *autoActivate=MyAny(true);
        manager.AddGlobalSceneVariable<std::string>("endLoadTarget",std::string("Origin"));
        manager.Update(engineKey);
        Check(manager.HasPendingSceneChange() && Current(manager)->GetName()=="RegressionLoading","script activation queued until frame end");
        Check(manager.CommitPendingSceneChange(engineKey),"prepared scene activated");
        Check(Current(manager)->GetName()=="RegressionDestination","target became current");
        Check(Current(manager)->GetSceneContext()->GetSceneObject("Editor only script")==nullptr,"EditorOnly objects removed before script activation");
        Check(manager.IsSceneLoading() && manager.GetSceneLoadTarget()=="Origin","loading-scene End can start another load without destroying the committed target");
        manager.CancelSceneLoad();
        Check(Counter(manager,"globalInit")==1 && Counter(manager,"ctor")==1 && Counter(manager,"awake")==1 && Counter(manager,"start")==0,"activation executes globals, constructor and Awake once");
        auto *script=Current(manager)->GetSceneContext()->GetSceneObject("Destination")->GetComponent<ScriptComponent>(); float configured=0;
        Check(script->GetVariable("configured",&configured,asTYPEID_FLOAT) && configured==39,"serialized script values survive preparation");
        manager.Update(engineKey); Check(Counter(manager,"start")==1 && Counter(manager,"destinationFrames")==1,"target Start and Update begin after activation");
        Check(manager.BeginSceneLoad("Target"),"second request started"); manager.CancelSceneLoad();
        Check(!manager.IsSceneLoading() && !manager.IsSceneLoadReady() && !manager.ActivateLoadedScene(),"read cancellation discards activation");
        Check(manager.BeginSceneLoad("Target"),"restart after cancellation"); Wait(manager); manager.CancelSceneLoad();
        Check(Counter(manager,"ctor")==1 && Counter(manager,"end")==0,"cancelled prepared scripts never construct or run End");
        manager.RegisterSceneFile("MissingFile",PathToUtf8String(folder/"absent.json")); Check(manager.BeginSceneLoad("MissingFile"),"missing file request queued");
        for(int i=0;i<1000 && manager.IsSceneLoading();++i) { manager.Update(engineKey); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        Check(!manager.IsSceneLoadReady() && !manager.GetSceneLoadError().empty() && Current(manager)->GetName()=="RegressionDestination","read failure keeps current scene and reports error");
        auto invalid=SceneData("Invalid"); auto invalidObject=Object("Invalid object"); invalidObject["components"].push_back(Component("MissingComponent",JSON::object())); invalid["sceneObjects"].push_back(invalidObject);
        manager.RegisterScene("Invalid",invalid); Check(manager.BeginSceneLoad("Invalid"),"invalid document request queued");
        for(int i=0;i<20 && manager.IsSceneLoading();++i)manager.Update(engineKey);
        Check(!manager.IsSceneLoadReady() && !manager.GetSceneLoadError().empty(),"unknown component fails preparation");
        auto duplicate=SceneData("Duplicate"); auto duplicateObject=Object("Duplicate"); duplicate["sceneObjects"]={duplicateObject,duplicateObject}; manager.RegisterScene("Duplicate",duplicate);
        Check(manager.BeginSceneLoad("Duplicate"),"duplicate UUID request queued"); manager.Update(engineKey);
        Check(!manager.IsSceneLoadReady() && !manager.GetSceneLoadError().empty(),"duplicate UUID fails preparation");
        std::ofstream("Bad.as") << "class Bad : ScriptComponentBehavior { void Update( { }";
        auto badScript=SceneData("Bad script"); auto badObject=Object("Bad script"); badObject["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(folder/"Bad.as")}})); badScript["sceneObjects"].push_back(badObject);
        manager.RegisterScene("Bad script",badScript); Check(manager.BeginSceneLoad("Bad script"),"bad script request queued");
        for(int i=0;i<20 && manager.IsSceneLoading();++i)manager.Update(engineKey);
        Check(!manager.IsSceneLoadReady() && !manager.GetSceneLoadError().empty(),"script compile error fails preparation");
        auto windowScene=SceneData("PreparedWindow"); auto windowObject=Object("Window"); windowObject["components"].push_back(Component("NormalWindowObject",{{"title","Must not appear during preparation"}})); windowScene["sceneObjects"].push_back(windowObject);
        manager.RegisterScene("WindowTarget",windowScene); Check(manager.BeginSceneLoad("WindowTarget"),"window target request queued"); Wait(manager);
        Check(Window::GetWindowCount()==0,"prepared target creates no native window"); manager.CancelSceneLoad();
        Check(manager.BeginSceneLoad("Target"),"ready cancellation setup"); Wait(manager);
        Check(manager.ChangeScene("Origin") && !manager.IsSceneLoadReady(),"ordinary scene change cancels prepared target");
        manager.CommitPendingSceneChange(engineKey);
        Check(manager.BeginSceneLoad("Target"),"unregister cancellation setup"); Wait(manager);
        Check(manager.UnregisterScene("Target") && !manager.IsSceneLoadReady(),"unregister target cancels job");
        manager.RegisterScene("Target",target); Check(manager.BeginSceneLoad("Target"),"Play Stop cancellation setup"); Wait(manager);
#if defined(USE_IMGUI)
        Current(manager)->PlayStop(); Check(!manager.IsSceneLoadReady() && !manager.IsSceneLoading(),"Play Stop cancels pending target");
#endif
        auto sample=SceneData("Example controller"); auto sampleObject=Object("Example controller"); sampleObject["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(original/"Docs/Examples/LoadingSceneController.as")}})); sample["sceneObjects"].push_back(sampleObject);
        manager.RegisterScene("Example controller",sample); Check(manager.ChangeScene("Example controller") && manager.CommitPendingSceneChange(engineKey),"example controller scene created");
        Check(Current(manager)->GetSceneContext()->GetSceneObject("Example controller")->GetComponent<ScriptComponent>()->GetLastError().empty(),"documented controller compiles against actual script APIs");
        std::ofstream("Shutdown.as") << "class Shutdown : ScriptComponentBehavior { void End() { int n=1; GetScene().SetGlobalVariable(\"shutdown\",n); GetScene().SaveGlobalVariables(" << JSON(PathToUtf8String(folder/"Shutdown.json")).dump() << "); } }";
        auto shutdown=SceneData("Shutdown"); auto shutdownObject=Object("Shutdown"); shutdownObject["components"].push_back(Component("ScriptComponent",{{"scriptPath",PathToUtf8String(folder/"Shutdown.as")}})); shutdown["sceneObjects"].push_back(shutdownObject);
        manager.RegisterScene("Shutdown",shutdown); Check(manager.ChangeScene("Shutdown") && manager.CommitPendingSceneChange(engineKey),"shutdown scene created");
        }
        Check(std::filesystem::is_regular_file(folder/"Shutdown.json"),"End can save global variables during manager shutdown");
        std::cout<<checks<<" scene loading checks passed\n";
        std::filesystem::current_path(original); return 0;
    }catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; std::filesystem::current_path(original); return 1; }
}
