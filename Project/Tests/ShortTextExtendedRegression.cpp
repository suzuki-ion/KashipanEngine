#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "ShortTextTestSceneManager.h"
using namespace KashipanEngine;
static void Check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static int Run(){try{
 auto manager=ShortTextTest::MakeSceneManager();Scene scene(std::string("ExtendedRegression"));ShortTextTest::AttachSceneManager(scene,*manager);auto *sc=scene.GetSceneContext();auto *owner=sc->CreateEmptyObject("subject");if(!owner->GetComponent<Transform>())owner->AddComponent<Transform>();auto *source=sc->CreateEmptyObject("source");auto *sourceTransform=source->GetComponent<Transform>();if(!sourceTransform)sourceTransform=source->AddComponent<Transform>();Check(sourceTransform!=nullptr,"Source transform");sourceTransform->SetScale({4,5,6});ShortText::ProgramMemory memory;JSON environment=JSON::object();ShortText::CommandContext c{owner,owner,sc,&memory,0,&environment};std::string error;
 auto execute=[&](const std::string &name,const JSON &args){ShortText::Action action;action.name=name;action.data=args;return ShortText::ApplyExtendedCommand(action,c,error);};
 Check(execute("variable.set",{{"name","goal"},{"type","bool"},{"value",true}}),"Bool variable set");Check(environment["scene:goal"]==true,"Same frame variable environment");
 Check(execute("variable.set",{{"scope","global"},{"name","title"},{"type","string"},{"value","sample"}}),"String variable set");Check(environment["global:title"]=="sample","Global environment");
 Check(execute("variable.set",{{"scope","global"},{"name","title"},{"value","updated"}})&&sc->GetGlobalSceneVariable("title")->AnyCast<std::string>()=="updated","Existing global variable is overwritten");
 Check(execute("variable.set",{{"name","a"},{"type","float"},{"value",12}})&&execute("variable.set",{{"name","b"},{"type","float"},{"value",3}}),"Numeric operands");
 Check(execute("variable.calculate",{{"name","result"},{"source","a"},{"other","b"},{"operator","divide"}})&&environment["scene:result"]==4,"Variable calculation");
 Check(execute("variable.set",{{"name","b"},{"value",0}})&&!execute("variable.calculate",{{"name","result"},{"source","a"},{"other","b"},{"operator","divide"}})&&sc->GetSceneVariable("result")->AnyCast<double>()==4,"Divide zero no mutation");
 Check(execute("property.set",{{"path","Transform.scale_.x"},{"value",JSON{{"read","scene:a"}}}})&&owner->GetComponent<Transform>()->GetScale().x==12,"Expression command args");Check(environment["property:self:Transform.scale_.x"]==12,"Same frame property environment");
 Check(!execute("property.sync",{{"path","Transform.scale_.y"},{"source","missing"},{"sourcePath","Transform.scale_.y"}}),"Missing source rejected");
 Check(execute("property.sync",{{"path","Transform.scale_.y"},{"source","source"},{"sourcePath","Transform.scale_.y"}})&&owner->GetComponent<Transform>()->GetScale().y==5,"Initial synchronization");source->GetComponent<Transform>()->SetScale({4,8,6});ShortText::ProgramState state;ShortText::ReadExtendedValues(owner,sc,state,&memory,0);Check(owner->GetComponent<Transform>()->GetScale().y==8,"Continuous synchronization");
 Check(execute("property.unsync",{{"path","Transform.scale_.y"}}),"Remove synchronization");
 auto *tile=owner->AddComponent<TilemapRenderer>();Check(execute("tilemap.resize",{{"width",8},{"height",8}})&&tile->GetGridWidth()==8,"Tile grid resize");Check(execute("tilemap.set",{{"x",1},{"y",1},{"tile",-1}}),"Clear cell");Check(!execute("tilemap.set",{{"x",9},{"y",1},{"tile",-1}}),"Invalid tile coordinate");
 auto *script=owner->AddComponent<ScriptComponent>();JSON component={{"type","ScriptComponent"},{"data",{{"customData",{{"fields",{{"goal",false},{"speed",2.0},{"label","old"}}}}}}}};Check(owner->LoadComponentFromJson(script,component),"Pending script fields load");JSON value;Check(script->GetSerializedVariable("goal",value)&&value==false,"Script pending getter");Check(script->SetSerializedVariable("goal",true)&&!script->SetSerializedVariable("goal",1),"Script strict bool setter");
 std::puts("ShortText extended regression passed");return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
int main(){auto original=std::filesystem::current_path();auto sandbox=std::filesystem::temp_directory_path()/("ShortTextExtended-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(sandbox);std::filesystem::current_path(sandbox);int result=Run();std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);return result;}
