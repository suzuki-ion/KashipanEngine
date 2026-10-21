#include <cstdio>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Utilities/ShortTextRecipeJson.h"
#include "ShortTextTestSceneManager.h"
using namespace KashipanEngine;
using namespace KashipanEngine::ShortText;
static void Check(bool b,const char *message){if(!b)throw std::runtime_error(message);}
int Run(){try{
 auto manager=ShortTextTest::MakeSceneManager();Scene scene(std::string("ExtendedSceneRegression"));ShortTextTest::AttachSceneManager(scene,*manager);auto *context=scene.GetSceneContext();
 auto *owner=context->CreateEmptyObject("Owner");auto *sprite=owner->AddComponent<SpriteRenderer>();
 auto *child=context->CreateEmptyObject("Child");child->GetComponent<Transform>()->SetParentObject(owner);
 context->AddSceneVariable<bool>("goal",false);
 Recipe r;r.kind=Kind::Rules;Rule fade;fade.event=Event::ConditionsEntered;
 Condition goal;goal.test=Test::Expression;goal.data={{"op","eq"},{"args",Value::array({Value{{"read","scene:goal"}},true})}};fade.conditions={goal};
 Action tween;tween.operation=Operation::TweenProperty;tween.name="SpriteRenderer.instanceColor_.a";tween.duration=1;tween.data={{"from",0},{"to",1}};fade.actions={tween};r.rules={fade};
 Check(DecodeRecipe(EncodeRecipe(r)).rules[0].actions[0].data==tween.data,"Extended recipe serialization");
 ProgramMemory memory;ProgramState state;InputSnapshot input;std::string error;
 auto tick=[&](double dt){state={};state.position={0,0,0};ReadShortTextSceneInput(owner,context,r,state,input,&memory,dt);TickProgram(r.rules,memory,state,input,dt);ApplyShortTextSceneEffects(owner,context,state,&memory,&error);if(!error.empty())throw std::runtime_error("Native command dispatch error: "+error);};
 tick(0.1);Check(state.commands.empty(),"False goal does not trigger fade");
 context->AddSceneVariable<bool>("goal",true);tick(0.25);Check(sprite->GetInstanceColor().w==0.25f,"Goal bool creates quarter fade");
 tick(0.75);Check(sprite->GetInstanceColor().w==1.0f,"Fade completes at one");
 tick(0.1);Check(state.commands.empty(),"Condition remains true without replay");
 Check(state.environment.at("object:Child").at("components").is_array(),"Component conditions snapshot");
 Check(FindObject(state,child->GetObjectID().ToString())->parent==owner->GetObjectID().ToString(),"Hierarchy snapshot");
 Rule generated;generated.event=Event::Start;Action make;make.operation=Operation::CreateObject;make.name="Generated";
 Action disable;disable.operation=Operation::SetActive;disable.target=Target::LastCreated;disable.value=0;generated.actions={make,disable};r.rules={generated};memory={};tick(0.1);
 auto made=context->GetSceneObjects("Generated");Check(made.size()==1&&!made[0]->IsActive(),"Same tick last-created target resolves");
 Rule typed;typed.event=Event::Start;Action first;first.operation=Operation::EngineCommand;first.name="variable.set";first.data={{"scope","global"},{"name","greeting"},{"type","string"},{"value","Hello"}};
 Action second=first;second.data={{"scope","scene"},{"name","greeting"},{"type","string"},{"value",Value{{"read","global:greeting"}}}};typed.actions={first,second};r.rules={typed};memory={};tick(0.1);
 Check(context->GetSceneVariable("greeting")->AnyCast<std::string>()=="Hello","Same tick native typed expressions");
 Rule mixed;mixed.event=Event::Start;Action score;score.operation=Operation::SetVariable;score.name="score";score.value=3;
 Action copy;copy.operation=Operation::EngineCommand;copy.name="variable.set";copy.data={{"name","scoreCopy"},{"type","float"},{"value",Value{{"read","scene:score"}}}};
 mixed.actions={score,copy};r.rules={mixed};memory={};tick(0.1);Check(context->GetSceneVariable("scoreCopy")->AnyCast<float>()==3,"Legacy variable setter updates same tick expression environment");
 Rule changed;changed.event=Event::VariableChanged;changed.variable="global:greeting";Action hide;hide.operation=Operation::SetVisible;hide.value=0;changed.actions={hide};r.rules={changed};memory={};tick(0.1);Check(sprite->IsActive(),"Global first read only records baseline");
 context->RemoveGlobalSceneVariable("greeting");context->AddGlobalSceneVariable<std::string>("greeting","Changed");tick(0.1);Check(!sprite->IsActive(),"Global string change triggers");
 // Reusing the snapshot cannot retain aliases/variables for removed objects or values.
 context->AddSceneVariable<std::string>("temporary","old");ReadShortTextSceneInput(owner,context,r,state,input,&memory,0.1);Check(state.environment.contains("scene:temporary"),"Temporary variable was observed");
 auto childID=child->GetObjectID().ToString();context->RemoveSceneVariable("temporary");context->DeleteObject(child);ReadShortTextSceneInput(owner,context,r,state,input,&memory,0.1);
 Check(!state.environment.contains("scene:temporary")&&!state.environment.contains("object:Child")&&!state.environment.contains("object:"+childID),"Removed snapshot entries disappear");
 // Nested parallel property tween without an explicit from must capture native current value.
 auto color=sprite->GetInstanceColor();color.w=0.4f;sprite->SetInstanceColor(color);
 Rule nested;nested.event=Event::Start;Action parallel;parallel.operation=Operation::Parallel;
 parallel.data={{"branches",Value::array({Value::array({Value{{"operation","tween_property"},{"name","SpriteRenderer.instanceColor_.a"},{"duration",1},{"data",{{"to",1}}}}}),Value::array({Value{{"operation","wait"},{"duration",1}}})})}};
 nested.actions={parallel};r.rules={nested};memory={};tick(0.5);Check(std::abs(sprite->GetInstanceColor().w-0.7f)<1e-5f,"Nested property tween captures current native alpha");
 tick(0.5);Check(sprite->GetInstanceColor().w==1,"Nested tween finishes");
 // Duplicate object names never expose an ambiguous name alias.
 auto *duplicate=context->CreateEmptyObject("Owner");ReadShortTextSceneInput(owner,context,r,state,input,&memory,0);
 Check(!state.environment.contains("object:Owner")&&state.environment.contains("object:self"),"Duplicate names have only stable ID/self aliases");context->DeleteObject(duplicate);
 std::puts("ShortText extended scene regression passed");return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
int main(){const auto original=std::filesystem::current_path();const auto sandbox=std::filesystem::temp_directory_path()/("ShortTextExtendedScene-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(sandbox);std::filesystem::current_path(sandbox);int result=Run();std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);return result;}
