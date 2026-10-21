#include <cstdio>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/CharacterController2D.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Render/Camera2D.h"
using namespace KashipanEngine;
static void Require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){
    auto original=std::filesystem::current_path();
    auto temp=std::filesystem::temp_directory_path()/("ShortTextPhysics-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);std::filesystem::current_path(temp);int code=0;
    try{
        Scene scene(std::string("PhysicsRecipes"));auto *context=scene.GetSceneContext();auto *object=context->CreateEmptyObject("Actor");
        auto *transform=object->GetComponent<Transform>();if(!transform)transform=object->AddComponent<Transform>();
        ShortText::ProgramMemory memory;ShortText::CommandContext c{object,object,context,&memory,0.1};std::string error;
        auto run=[&](const std::string &cmd,const ShortText::Value &args){error.clear();return ShortText::ExecutePhysicsCommand(cmd,args,c,error);};
        Require(!run("unknown",{} )&&error.empty(),"unknown command must allow next adapter");
        Require(!run("controller_move",{{"velocity",{1,0,0}}})&&!error.empty(),"missing controller fails explicitly");
        auto *controller=object->AddComponent<CharacterController2D>();Require(controller!=nullptr,"controller registration");
        Require(run("controller_move",{{"velocity",{1,0,0}}}),"controller accepts displacement request");
        Require(run("physics_state",{{"result","state"}}),"controller state read");Require(memory.extensionValues["results"]["state"]["grounded"]==false,"unmoved controller is not grounded");
        auto *rigid=object->AddComponent<RigidBody2D>();Require(rigid!=nullptr,"rigid registration");
        Require(run("rigid_settings",{{"mass",2},{"gravity",false}}),"rigid settings path");Require(rigid->GetMass()==2&&!rigid->IsGravityEnabled(),"settings update engine component");
        Require(!run("rigid_settings",{{"mass",-1}}),"negative mass rejected");Require(rigid->GetMass()==2,"failed mass retains value");
        // CharacterController2D created the scene physics world, so this body is live.
        Require(run("rigid_force",{{"value",{2,0,0}}}),"initialized physical body accepts force");
        {
            Scene bare(std::string("PhysicsNoWorld"));auto *bareContext=bare.GetSceneContext();auto *bareObject=bareContext->CreateEmptyObject("Body");
            if(!bareObject->GetComponent<Transform>())bareObject->AddComponent<Transform>();Require(bareObject->AddComponent<RigidBody2D>()!=nullptr,"bare rigid registration");
            ShortText::CommandContext bc{bareObject,bareObject,bareContext,&memory,0.1};error.clear();
            Require(!ShortText::ExecutePhysicsCommand("rigid_force",{{"value",{2,0,0}}},bc,error)&&!error.empty(),"uninitialized physical body fails instead of silently ignoring force");
        }
        Require(run("look_at",{{"position",{0,1,0}}}),"look at vector");Require(std::abs(transform->GetRotate().z-1.5707963f)<1e-5f,"2D facing angle");
        Require(run("accelerated_move",{{"position",{1,0,0}},{"speed",5},{"acceleration",20}}),"accelerated move command");Require(transform->GetTranslate().x>0&&transform->GetTranslate().x<1,"accelerated movement advances without teleporting");
        transform->SetTranslate({0,0,0});for(int i=0;i<100;++i)Require(run("accelerated_move",{{"position",{1,0,0}},{"speed",5},{"acceleration",20}}),"advance accelerated movement");
        Require(std::abs(transform->GetTranslate().x-1)<0.011f,"movement decelerates and arrives");Require(memory.extensionValues["results"]["physics"]["arrived"]==true,"arrival signal");
        Require(run("grid_path",{{"start",{0,0}},{"goal",{3,0}},{"width",4},{"height",3},{"blocked",{{1,0},{1,1}}},{"result","route"}}),"bounded path query");
        auto route=memory.extensionValues["results"]["route"];Require(route["found"]==true&&route["points"].size()>4,"path avoids blocked cells");
        Require(run("grid_path",{{"start",{0,0}},{"goal",{3,0}},{"width",4},{"height",3},{"blocked",{{1,0},{1,1},{1,2}}},{"result","route"}}),"unreachable path query succeeds");Require(memory.extensionValues["results"]["route"]["found"]==false,"unreachable path explicit result");
        Require(!run("grid_path",{{"start",{0,0}},{"goal",{3,0}},{"width",10000}}),"path size budget");
        auto *camera=context->CreateEmptyObject("Camera");auto *ct=camera->GetComponent<Transform>();if(!ct)ct=camera->AddComponent<Transform>();ct->SetTranslate({2,3,0});camera->AddComponent<Camera2D>()->SetSize(100,200);
        Require(run("mouse_world",{{"camera","Camera"},{"uv",{0.5,0.5}},{"result","world"}}),"camera UV conversion");auto world=memory.extensionValues["results"]["world"];Require(std::abs(world[0].get<float>()-52)<1e-4f&&std::abs(world[1].get<float>()-103)<1e-4f,"UV transforms through camera");
        Require(!run("look_at",{{"position",{0,0,0,0}}}),"invalid vector rejected");
        std::puts("Short-text physics/input native regression passed");
    }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());code=1;}
    std::filesystem::current_path(original);std::filesystem::remove_all(temp);return code;
}
