#include "ShortTextExtended.h"
#include "Scene/SceneContext.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "Math/Color.h"
#include <cmath>
#include <limits>
#include <climits>

namespace KashipanEngine::ShortText {
namespace {
Value ToValue(const MyAny &v) {
 if(v.IsType<bool>())return v.AnyCast<bool>();if(v.IsType<int>())return v.AnyCast<int>();if(v.IsType<unsigned>())return v.AnyCast<unsigned>();if(v.IsType<int64_t>())return v.AnyCast<int64_t>();if(v.IsType<uint64_t>())return v.AnyCast<uint64_t>();if(v.IsType<float>())return v.AnyCast<float>();if(v.IsType<double>())return v.AnyCast<double>();if(v.IsType<std::string>())return v.AnyCast<std::string>();
 if(v.IsType<Vector2>()){auto x=v.AnyCast<Vector2>();return Value::array({x.x,x.y});}if(v.IsType<Vector3>()){auto x=v.AnyCast<Vector3>();return Value::array({x.x,x.y,x.z});}if(v.IsType<Vector4>()){auto x=v.AnyCast<Vector4>();return Value::array({x.x,x.y,x.z,x.w});}if(v.IsType<Color>()){auto x=v.AnyCast<Color>();return Value::array({x.r,x.g,x.b,x.a});}if(v.IsType<UUID128>())return v.AnyCast<UUID128>().ToString();return nullptr;
}
std::string Type(const MyAny *v){if(!v)return {};if(v->IsType<bool>())return "bool";if(v->IsType<int>())return "int";if(v->IsType<unsigned>())return "uint";if(v->IsType<int64_t>())return "int64";if(v->IsType<uint64_t>())return "uint64";if(v->IsType<float>())return "float";if(v->IsType<double>())return "double";if(v->IsType<std::string>())return "string";if(v->IsType<Vector2>())return "vector2";if(v->IsType<Vector3>())return "vector3";if(v->IsType<Vector4>())return "vector4";if(v->IsType<Color>())return "color";if(v->IsType<UUID128>())return "object";return {};}
MyAny *Var(SceneContext *scene,const std::string &scope,const std::string &name){return scope=="global"?scene->GetGlobalSceneVariable(name):scope=="scene"?scene->GetSceneVariable(name):nullptr;}
// SceneManager::AddGlobalSceneVariable never overwrites an existing key, so replace explicitly.
template<class T> void Store(SceneContext *scene,const std::string &scope,const std::string &name,const T &value){if(scope=="global"){scene->RemoveGlobalSceneVariable(name);scene->AddGlobalSceneVariable<T>(name,value);}else scene->AddSceneVariable<T>(name,value);}
bool Finite(const Value &v){if(v.is_number())return std::isfinite(v.get<double>());if(v.is_array()||v.is_object())for(const auto &item:v)if(!Finite(item))return false;return true;}
bool SetVar(SceneContext *scene,const std::string &scope,const std::string &name,const std::string &type,const Value &v){
 if(name.empty()||name.size()>256||(scope!="scene"&&scope!="global")||!Finite(v))return false;
 if(type=="bool"){if(!v.is_boolean())return false;Store(scene,scope,name,v.get<bool>());return true;}if(type=="string"){if(!v.is_string())return false;Store(scene,scope,name,v.get<std::string>());return true;}
 if(type=="object"){if(!v.is_string())return false;UUID128 id(v.get<std::string>());if(!id.IsValid())return false;Store(scene,scope,name,id);return true;}
 if(type=="vector2"||type=="vector3"||type=="vector4"||type=="color"){
  int n=type=="vector2"?2:type=="vector3"?3:4;float a[4]{};const char *keys[]={"x","y","z","w"};const char *rgba[]={"r","g","b","a"};if(!v.is_array()&&!v.is_object())return false;if(v.is_array()&&v.size()!=static_cast<size_t>(n))return false;
  for(int i=0;i<n;++i){const Value *part=nullptr;if(v.is_array())part=&v[i];else if(v.contains(keys[i]))part=&v.at(keys[i]);else if(v.contains(rgba[i]))part=&v.at(rgba[i]);if(!part||!part->is_number()||std::abs(part->get<double>())>std::numeric_limits<float>::max())return false;a[i]=part->get<float>();}
  if(n==2)Store(scene,scope,name,Vector2(a[0],a[1]));else if(n==3)Store(scene,scope,name,Vector3(a[0],a[1],a[2]));else if(type=="color")Store(scene,scope,name,Color(a[0],a[1],a[2],a[3]));else Store(scene,scope,name,Vector4(a[0],a[1],a[2],a[3]));return true;
 }
 if(!v.is_number())return false;const auto number=v.get<long double>();if(type=="int"&&number==std::trunc(number)&&number>=INT_MIN&&number<=INT_MAX){Store(scene,scope,name,v.get<int>());return true;}if(type=="uint"&&number==std::trunc(number)&&number>=0&&number<=UINT_MAX){Store(scene,scope,name,v.get<unsigned>());return true;}
 if(type=="int64"&&number==std::trunc(number)&&number>=std::numeric_limits<int64_t>::min()&&number<=std::numeric_limits<int64_t>::max()){Store(scene,scope,name,v.get<int64_t>());return true;}if(type=="uint64"&&number==std::trunc(number)&&number>=0&&number<=std::numeric_limits<uint64_t>::max()){Store(scene,scope,name,v.get<uint64_t>());return true;}
 if(type=="float"&&std::abs(number)<=std::numeric_limits<float>::max()){Store(scene,scope,name,v.get<float>());return true;}if(type=="double"){Store(scene,scope,name,v.get<double>());return true;}return false;
}
std::string Infer(const Value &v){if(v.is_boolean())return "bool";if(v.is_string())return "string";if(v.is_number())return "double";if(v.is_array()&&v.size()>=2&&v.size()<=4)return "vector"+std::to_string(v.size());return {};}
EmptyObject *Source(SceneContext *scene,const Value &a){if(!scene)return nullptr;auto name=a.value("source",std::string{});if(name.empty()||name=="self")return nullptr;UUID128 id(name);if(id.IsValid())return scene->GetSceneObject(id);auto matches=scene->GetSceneObjects(name);return matches.size()==1?matches.front():nullptr;}
bool Arithmetic(const Value &lhs,const Value &rhs,const std::string &op,Value &result){
 if(lhs.is_array()){if(rhs.is_array()&&lhs.size()!=rhs.size())return false;result=Value::array();for(size_t i=0;i<lhs.size();++i){Value x;if(!Arithmetic(lhs[i],rhs.is_array()?rhs[i]:rhs,op,x))return false;result.push_back(x);}return true;}
 if(op=="add"&&lhs.is_string()&&rhs.is_string()){result=lhs.get<std::string>()+rhs.get<std::string>();return true;}if(!lhs.is_number()||!rhs.is_number())return false;double x=lhs.get<double>(),y=rhs.get<double>(),z=0;if(op=="add")z=x+y;else if(op=="subtract")z=x-y;else if(op=="multiply")z=x*y;else if(op=="divide"){if(y==0)return false;z=x/y;}else return false;if(!std::isfinite(z))return false;result=z;return true;
}
}
static bool ApplyCommandResolved(const Action &action,CommandContext &c,std::string &error){
 error.clear();const auto &cmd=action.name;const auto &a=action.data;
 try{
  if(!a.is_object()){error="Command arguments must be an object";return false;}
  if(cmd=="property.set"||cmd=="property.sync"||cmd=="property.unsync"){
   if(!c.target){error="Target unavailable";return false;}const auto path=a.at("path").get<std::string>();
   if(cmd=="property.set"){Value value;if(a.contains("value"))value=a.at("value");else {auto *source=Source(c.scene,a);if(!source){if(a.contains("source")&&a.at("source")!="self"&&!a.at("source").get<std::string>().empty()){error="Source object unavailable";return false;}source=c.target;}if(!ReadExtendedProperty(source,a.at("sourcePath").get<std::string>(),value)){error="Source property unavailable";return false;}}if(!WriteExtendedProperty(c.target,path,value)){error="Property rejected value";return false;}return true;}
   if(!c.memory){error="Synchronization requires program memory";return false;}auto key=c.target->GetObjectID().ToString()+":"+path;auto &syncs=c.memory->extensionValues["propertySyncs"];if(!syncs.is_object())syncs=Value::object();if(cmd=="property.unsync"){syncs.erase(key);return true;}
   auto *source=Source(c.scene,a);if(!source){if(a.contains("source")&&a.at("source")!="self"&&!a.at("source").get<std::string>().empty()){error="Source object unavailable";return false;}source=c.target;}Value value;if(!ReadExtendedProperty(source,a.at("sourcePath").get<std::string>(),value)||!WriteExtendedProperty(c.target,path,value)){error="Synchronization property unavailable";return false;}syncs[key]={{"target",c.target->GetObjectID().ToString()},{"source",source->GetObjectID().ToString()},{"path",path},{"sourcePath",a.at("sourcePath")}};return true;
  }
  if(cmd.starts_with("variable.")){
   if(!c.scene){error="Scene unavailable";return false;}auto scope=a.value("scope",std::string("scene"));auto name=a.at("name").get<std::string>();auto *old=Var(c.scene,scope,name);
   if(cmd=="variable.get"){if(!old){error="Variable unavailable";return false;}if(c.memory)c.memory->extensionValues["results"][a.value("result",name)]=ToValue(*old);return true;}
   if(cmd=="variable.set"||cmd=="variable.copy"||cmd=="variable.calculate"){
    Value value;if(cmd=="variable.set")value=a.at("value");else {auto sourceScope=a.value("sourceScope",scope);auto *other=Var(c.scene,sourceScope,a.at("source").get<std::string>());if(!other){error="Source variable unavailable";return false;}value=ToValue(*other);if(cmd=="variable.calculate"){Value rhs;if(a.contains("other")){auto *v=Var(c.scene,a.value("otherScope",scope),a.at("other").get<std::string>());if(!v){error="Second variable unavailable";return false;}rhs=ToValue(*v);}else rhs=a.at("value");Value computed;if(!Arithmetic(value,rhs,a.at("operator").get<std::string>(),computed)){error="Calculation operands invalid";return false;}value=computed;}}
    auto type=a.value("type",old?Type(old):Infer(value));if(!SetVar(c.scene,scope,name,type,value)){error="Variable value/type invalid";return false;}return true;
   }
  }
  if(cmd.starts_with("tilemap.")){
   auto *tile=c.target?c.target->GetComponent<TilemapRenderer>():nullptr;if(!tile){error="TilemapRenderer unavailable";return false;}
   if(cmd=="tilemap.clear"){tile->Clear();return true;}if(cmd=="tilemap.resize"){int w=a.at("width").get<int>(),h=a.at("height").get<int>();if(w<0||h<0||static_cast<int64_t>(w)*h>1048576){error="Grid bounds exceeded";return false;}tile->Resize(w,h);return true;}
   if(cmd=="tilemap.set"||cmd=="tilemap.get"){int x=a.at("x").get<int>(),y=a.at("y").get<int>();if(x<0||y<0||x>=tile->GetGridWidth()||y>=tile->GetGridHeight()){error="Cell out of range";return false;}if(cmd=="tilemap.get"){if(c.memory)c.memory->extensionValues["results"][a.value("result",std::string("tile"))]=tile->GetTile(x,y);return true;}int type=a.at("tile").get<int>();if(type< -1||type>=tile->GetTileTypeCount()){error="Tile type out of range";return false;}tile->SetTile(x,y,type);return true;}
   if(cmd=="tilemap.solid"){int type=a.at("tile").get<int>();if(type<0||type>=tile->GetTileTypeCount()){error="Tile type out of range";return false;}tile->SetTileTypeSolid(type,a.at("solid").get<bool>());return true;}
  }
  if(ExecuteRenderingCommand(cmd,a,c,error)||!error.empty())return error.empty();
  if(ExecutePhysicsCommand(cmd,a,c,error)||!error.empty())return error.empty();
  if(ExecuteSpatialCommand(cmd,a,c,error)||!error.empty())return error.empty();
  if(ExecuteMediaCommand(cmd,a,c,error)||!error.empty())return error.empty();
  error="Unsupported command: "+cmd;return false;
 }catch(const std::exception &e){error=e.what();return false;}
}
namespace {
Value ResolveArguments(const Value &value,const Value &environment,int depth=0){
 if(depth>32)throw std::runtime_error("Command argument nesting exceeds limit");
 if(value.is_object()){
  if(value.contains("read")||value.contains("op")||value.contains("literal"))return Evaluate(value,environment);
  Value result=Value::object();for(auto &[key,item]:value.items())result[key]=ResolveArguments(item,environment,depth+1);return result;
 }
 if(value.is_array()){Value result=Value::array();for(const auto &item:value)result.push_back(ResolveArguments(item,environment,depth+1));return result;}return value;
}
}
bool ApplyExtendedCommand(const Action &action,CommandContext &context,std::string &error){
 try{
  Action resolved=action;if(context.environment)resolved.data=ResolveArguments(action.data,*context.environment);
  const bool ok=ApplyCommandResolved(resolved,context,error);if(!ok)return false;
  if(context.environment){auto &env=*context.environment;
   if(context.scene){for(const auto &[name,value]:context.scene->GetSceneVariables())env["scene:"+name]=ToValue(value);for(const auto &[name,value]:context.scene->GetGlobalSceneVariables())env["global:"+name]=ToValue(value);}
   if(context.memory&&context.memory->extensionValues.contains("results"))env["results"]=context.memory->extensionValues["results"];
   if(context.target&&resolved.name.starts_with("property.")&&resolved.data.contains("path")){auto path=resolved.data.at("path").get<std::string>();Value value;if(ReadExtendedProperty(context.target,path,value)){env["property:"+context.target->GetObjectID().ToString()+":"+path]=value;if(context.scene&&context.scene->GetSceneObjects(context.target->GetName()).size()==1)env["property:"+context.target->GetName()+":"+path]=value;if(context.owner==context.target)env["property:self:"+path]=value;}}
  }return true;
 }catch(const std::exception &e){error=e.what();return false;}
}
void ReadExtendedValues(const EmptyObject *owner,SceneContext *scene,ProgramState &state,ProgramMemory *memory,double dt){
 if(scene){for(const auto &[name,value]:scene->GetSceneVariables())state.environment["scene:"+name]=ToValue(value);for(const auto &[name,value]:scene->GetGlobalSceneVariables())state.environment["global:"+name]=ToValue(value);}
 if(scene&&memory&&memory->extensionValues.contains("propertySyncs"))for(const auto &entry:memory->extensionValues["propertySyncs"]){auto *target=scene->GetSceneObject(UUID128(entry.value("target",std::string{})));auto *source=scene->GetSceneObject(UUID128(entry.value("source",std::string{})));Value value;if(ReadExtendedProperty(source,entry.value("sourcePath",std::string{}),value))WriteExtendedProperty(target,entry.value("path",std::string{}),value);}
 ReadPhysicsValues(owner,scene,state,memory,dt);ReadMediaValues(owner,scene,state,memory,dt);if(memory&&memory->extensionValues.contains("results"))state.environment["results"]=memory->extensionValues["results"];
}
}
