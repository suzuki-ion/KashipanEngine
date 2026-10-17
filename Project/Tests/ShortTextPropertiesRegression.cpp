#include <cstdio>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Math/Color.h"
using namespace KashipanEngine;
namespace KashipanEngine {
class PropertyTest final:public IObjectComponent {
public:
 OBJECT_COMPONENT_CONSTRUCTOR(PropertyTest,0xFF,
  AddMemberVariable("value_",&number,[this]{++notifications;});
  AddMemberVariable("vector_",&vector);
  AddMemberVariable("color_",&color);
  AddMemberVariable("params_.amount",&amount);
  AddMemberVariable("count_",&count);
  AddMemberVariable("enabled_",&enabled);
  AddMemberVariable("text_",&text);
  AddReadOnlyMemberVariable("locked_",&locked);
 )
 std::unique_ptr<IObjectComponent> Clone()const override{return std::make_unique<PropertyTest>();}
 float number=1,amount=3;Vector3 vector{1,2,3};Color color{0.1f,0.2f,0.3f,1};int count=2,notifications=0;bool enabled=true;std::string text="test";const float locked=5;
};
}
static void Check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int Run(){try{
 // Component pools are registered by the first constructor call; production components are registered elsewhere.
 {PropertyTest registration;(void)registration;}
 Scene scene(std::string("PropertyRegression"));auto *o=scene.GetSceneContext()->CreateEmptyObject("subject");auto *first=o->AddComponent<PropertyTest>();auto *second=o->AddComponent<PropertyTest>();JSON value;
 Check(ShortText::ReadExtendedProperty(o,"PropertyTest.vector_.y",value)&&value==2,"Vector component read");
 Check(ShortText::WriteExtendedProperty(o,"PropertyTest.value_",4.0)&&first->number==4&&first->notifications==1,"Setter callback called");
 Check(!ShortText::WriteExtendedProperty(o,"PropertyTest.value_","bad")&&first->number==4&&first->notifications==1,"Type mismatch no mutation");
 Check(!ShortText::WriteExtendedProperty(o,"PropertyTest.locked_",7)&&first->locked==5,"Read-only protected");
 Check(!ShortText::WriteExtendedProperty(o,"PropertyTest.count_",2.5)&&first->count==2,"Fractional integer rejected");
 Check(!ShortText::WriteExtendedProperty(o,"PropertyTest.enabled_",1)&&first->enabled,"Boolean requires bool");
 Check(ShortText::WriteExtendedProperty(o,"PropertyTest[1].vector_",JSON{{"x",4},{"y",5},{"z",6}})&&second->vector.z==6&&first->vector.z==3,"Object vector and indexed component");
 Check(ShortText::WriteExtendedProperty(o,"PropertyTest.color_.a",0.5)&&first->color.a==0.5f&&first->color.r==0.1f,"Color alpha preserves channels");
 Check(ShortText::WriteExtendedProperty(o,"PropertyTest.params_.amount",7)&&first->amount==7,"Dotted member key prioritized");
 Check(!ShortText::WriteExtendedProperty(o,"PropertyTest.vector_.w",3)&&first->vector.z==3,"Out of bounds vector component rejected");
 Check(ShortText::WriteExtendedProperty(o,"object.active",false)&&!o->IsActive(),"Object active setter");
 Check(ShortText::ReadExtendedProperty(nullptr,"object.exists",value)&&value==false,"Null object exists false");
 std::puts("ShortText property regression passed");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}

int main(){const auto original=std::filesystem::current_path();auto sandbox=std::filesystem::temp_directory_path()/("ShortTextProperties-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(sandbox);std::filesystem::current_path(sandbox);int result=Run();std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);return result;}
