#include <chrono>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "ShortTextTestSceneManager.h"
using namespace KashipanEngine;
static void Check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){
 auto original=std::filesystem::current_path();auto sandbox=std::filesystem::temp_directory_path()/("ShortTextMedia-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(sandbox);std::filesystem::current_path(sandbox);int status=0;
 try{
  auto manager=ShortTextTest::MakeSceneManager();Scene scene(std::string("MediaRegression"));ShortTextTest::AttachSceneManager(scene,*manager);auto *sc=scene.GetSceneContext();auto *target=sc->CreateEmptyObject("template");target->AddComponent<Transform>();ShortText::ProgramMemory memory;ShortText::CommandContext c{target,target,sc,&memory,0};std::string error;
  Check(!ShortText::ExecuteMediaCommand("unsupported",JSON::object(),c,error)&&error.empty(),"Unknown command falls through");
  Check(!ShortText::ExecuteMediaCommand("audio.pause",JSON::object(),c,error)&&!error.empty(),"Missing source reported");
  auto *audio=target->AddComponent<AudioSource>();Check(ShortText::ExecuteMediaCommand("audio.configure",{{"volume",0.25},{"pitch",3},{"loop",true},{"spatial",true}},c,error),"Audio configure");Check(audio->GetVolume()==0.25f&&audio->GetPitch()==3&&audio->GetLoop()&&audio->GetEnableSpatialAudio(),"Audio settings applied");
  Check(ShortText::ExecuteMediaCommand("event.emit",{{"name","open"}},c,error),"Event emit");Check(sc->GetSceneVariable("__shortTextEvent.open")->AnyCast<int>()==1,"Event sequence");sc->AddSceneVariable<int>("__shortTextEvent.open",INT_MAX);Check(ShortText::ExecuteMediaCommand("event.emit",{{"name","open"}},c,error)&&sc->GetSceneVariable("__shortTextEvent.open")->AnyCast<int>()==1,"Event serial overflow wraps safely");
  Check(!ShortText::ExecuteMediaCommand("save.write",{{"slot","../bad"}},c,error),"Save path traversal rejected");
  sc->AddGlobalSceneVariable<std::string>("playerName","test");Check(ShortText::ExecuteMediaCommand("save.write",{{"slot","test"}},c,error),"Typed save");sc->RemoveGlobalSceneVariable("playerName");sc->AddGlobalSceneVariable<std::string>("playerName","changed");Check(sc->GetGlobalSceneVariable("playerName")->AnyCast<std::string>()=="changed","Global value changed before load");Check(ShortText::ExecuteMediaCommand("save.read",{{"slot","test"}},c,error),"Typed load");Check(sc->GetGlobalSceneVariable("playerName")->AnyCast<std::string>()=="test","Typed data restored");
  std::ofstream out("prefab.json");out<<JSON{{"prefab",true},{"objects",JSON::array({JSON{{"parentIndex",-1},{"object",sc->SaveObjectToJson(target)}}})}}.dump();out.close();
  Check(ShortText::ExecuteMediaCommand("prefab.spawn",{{"path","prefab.json"},{"position",JSON::array({1,2,3})}},c,error),"Runtime prefab spawn");auto *spawn=sc->GetSceneObject(UUID128(memory.extensionValues["lastSpawned"].get<std::string>()));Check(spawn&&spawn!=target&&spawn->GetComponent<Transform>()->GetTranslate().y==2,"Prefab pose and identity");Check(memory.extensionValues["last_created"]==memory.extensionValues["lastSpawned"],"Core last-created selection updated");
  const auto beforeMissing=sc->GetSceneObjects().size();Check(!ShortText::ExecuteMediaCommand("prefab.spawn",{{"path","prefab.json"},{"parent","missing"}},c,error)&&sc->GetSceneObjects().size()==beforeMissing,"Missing parent rejects before creation");
  auto *parent=sc->CreateEmptyObject("parent");parent->AddComponent<Transform>();Check(ShortText::ExecuteMediaCommand("prefab.spawn",{{"path","prefab.json"},{"parent",parent->GetObjectID().ToString()}},c,error),"Parent ID resolves");auto *parented=sc->GetSceneObject(UUID128(memory.extensionValues["last_created"].get<std::string>()));Check(parented->GetComponent<Transform>()->GetParentObject()==parent,"Prefab parent assigned");
  c.target=parented;Check(!ShortText::ExecuteMediaCommand("object.persist",{{"key","child"}},c,error)&&!sc->GetGlobalSceneVariable("__shortTextObject.child"),"Child persistence rejects without global mutation");c.target=target;
  auto *duplicate=sc->CreateEmptyObject("parent");(void)duplicate;Check(!ShortText::ExecuteMediaCommand("prefab.spawn",{{"path","prefab.json"},{"parent","parent"}},c,error),"Ambiguous parent rejected");
  Check(ShortText::ExecuteMediaCommand("pool.acquire",{{"pool","test"},{"limit",1}},c,error),"Pool acquire");auto id=memory.extensionValues["lastSpawned"].get<std::string>();Check(!ShortText::ExecuteMediaCommand("pool.acquire",{{"pool","test"},{"limit",1}},c,error),"Pool bound");c.target=sc->GetSceneObject(UUID128(id));Check(ShortText::ExecuteMediaCommand("pool.release",{{"pool","test"}},c,error),"Pool release");c.target=target;Check(ShortText::ExecuteMediaCommand("pool.acquire",{{"pool","test"},{"limit",1}},c,error)&&memory.extensionValues["lastSpawned"]==id,"Pool stable identity reused");
  std::puts("ShortText media regression passed");
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());status=1;}
 std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);return status;
}
