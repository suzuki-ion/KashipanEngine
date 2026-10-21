#include "ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#include <cmath>
#include <limits>

namespace KashipanEngine::ShortText {
namespace {
struct Property { IObjectComponent *component=nullptr; std::string member; int channel=-1; };
int Channel(const std::string &s){if(s=="x"||s=="r")return 0;if(s=="y"||s=="g")return 1;if(s=="z"||s=="b")return 2;if(s=="w"||s=="a")return 3;return -1;}
Property Locate(const EmptyObject *object,const std::string &path){
 Property p;if(!object)return p;auto dot=path.find('.');if(dot==std::string::npos)return p;auto type=path.substr(0,dot);int index=0;
 auto bracket=type.find('[');if(bracket!=std::string::npos){if(type.back()!=']')return p;auto text=type.substr(bracket+1,type.size()-bracket-2);if(text.empty()||text.size()>6||text.find_first_not_of("0123456789")!=std::string::npos)return p;index=std::stoi(text);type.resize(bracket);}
 for(auto &[component,unused]:object->GetAllComponents())if(component&&component->GetComponentType()==type){if(index--==0){p.component=component;break;}}
 if(!p.component)return p;p.member=path.substr(dot+1);
 if(p.component->GetMemberVariable(p.member))return p;
 auto last=p.member.rfind('.');if(last!=std::string::npos){int channel=Channel(p.member.substr(last+1));auto base=p.member.substr(0,last);if(channel>=0&&(p.component->GetMemberVariable(base)||dynamic_cast<ScriptComponent*>(p.component))){p.member=base;p.channel=channel;}}
 return p;
}
template<class T> bool ScalarRead(const Property &p,Value &out){T value{};if(!p.component->GetMemberValue(p.member,value))return false;out=value;return true;}
bool RawRead(const Property &p,Value &out){
 if(!p.component)return false;
 if(auto *script=dynamic_cast<ScriptComponent*>(p.component);script&&!p.component->GetMemberVariable(p.member))if(script->GetSerializedVariable(p.member,out))return true;
 if(ScalarRead<bool>(p,out)||ScalarRead<int>(p,out)||ScalarRead<unsigned>(p,out)||ScalarRead<long>(p,out)||ScalarRead<unsigned long>(p,out)||ScalarRead<long long>(p,out)||ScalarRead<unsigned long long>(p,out)||ScalarRead<float>(p,out)||ScalarRead<double>(p,out)||ScalarRead<std::string>(p,out))return true;
 Vector2 v2;if(p.component->GetMemberValue(p.member,v2)){out=Value::array({v2.x,v2.y});return true;}
 Vector3 v3;if(p.component->GetMemberValue(p.member,v3)){out=Value::array({v3.x,v3.y,v3.z});return true;}
 Vector4 v4;if(p.component->GetMemberValue(p.member,v4)){out=Value::array({v4.x,v4.y,v4.z,v4.w});return true;}
 Color color;if(p.component->GetMemberValue(p.member,color)){out=Value::array({color.r,color.g,color.b,color.a});return true;}return false;
}
bool Number(const Value &v,double &number){if(!v.is_number())return false;number=v.get<double>();return std::isfinite(number);}
template<class T> bool NumericWrite(const Property &p,const Value &v){
 T existing{};if(!p.component->GetMemberValue(p.member,existing))return false;double n;if(!Number(v,n))return false;
 if constexpr(std::is_integral_v<T>){if(std::trunc(n)!=n||static_cast<long double>(n)<static_cast<long double>(std::numeric_limits<T>::lowest())||static_cast<long double>(n)>static_cast<long double>(std::numeric_limits<T>::max()))return false;}
 else if(n < -std::numeric_limits<T>::max()||n>std::numeric_limits<T>::max())return false;
 return p.component->SetMemberValue(p.member,static_cast<T>(n));
}
bool Array(const Value &value,int size,float *out){
 static const char *xyz[]={"x","y","z","w"};static const char *rgba[]={"r","g","b","a"};
 if(value.is_array()&&value.size()!=static_cast<size_t>(size))return false;if(!value.is_array()&&!value.is_object())return false;
 for(int i=0;i<size;++i){const Value *v=nullptr;if(value.is_array())v=&value[i];else if(value.contains(xyz[i]))v=&value.at(xyz[i]);else if(value.contains(rgba[i]))v=&value.at(rgba[i]);if(!v)return false;double n;if(!Number(*v,n)||std::abs(n)>std::numeric_limits<float>::max())return false;out[i]=static_cast<float>(n);}return true;
}
bool RawWrite(const Property &p,const Value &value){
 if(!p.component)return false;
 if(auto *script=dynamic_cast<ScriptComponent*>(p.component);script&&!p.component->GetMemberVariable(p.member))return script->SetSerializedVariable(p.member,value);
 const auto *member=p.component->GetMemberVariable(p.member);if(!member||!member->IsWritable())return false;
 bool boolean{};if(p.component->GetMemberValue(p.member,boolean))return value.is_boolean()&&p.component->SetMemberValue(p.member,value.get<bool>());
 std::string text;if(p.component->GetMemberValue(p.member,text))return value.is_string()&&p.component->SetMemberValue(p.member,value.get<std::string>());
 if(NumericWrite<int>(p,value)||NumericWrite<unsigned>(p,value)||NumericWrite<long>(p,value)||NumericWrite<unsigned long>(p,value)||NumericWrite<long long>(p,value)||NumericWrite<unsigned long long>(p,value)||NumericWrite<float>(p,value)||NumericWrite<double>(p,value))return true;
 float a[4];Vector2 v2;if(p.component->GetMemberValue(p.member,v2))return Array(value,2,a)&&p.component->SetMemberValue(p.member,Vector2(a[0],a[1]));
 Vector3 v3;if(p.component->GetMemberValue(p.member,v3))return Array(value,3,a)&&p.component->SetMemberValue(p.member,Vector3(a[0],a[1],a[2]));
 Vector4 v4;if(p.component->GetMemberValue(p.member,v4))return Array(value,4,a)&&p.component->SetMemberValue(p.member,Vector4(a[0],a[1],a[2],a[3]));
 Color color;if(p.component->GetMemberValue(p.member,color))return Array(value,4,a)&&p.component->SetMemberValue(p.member,Color(a[0],a[1],a[2],a[3]));return false;
}
}
bool ReadExtendedProperty(const EmptyObject *object,const std::string &path,Value &value){
 try{
  if(path=="object.exists"){value=object!=nullptr;return true;}if(!object)return false;
  if(path=="object.active"){value=object->IsActive();return true;}
  if(path=="ScreenBufferObject.width"||path=="ScreenBufferObject.height"){auto *buffer=object->GetComponent<ScreenBufferObject>();if(!buffer)return false;const bool width=path.ends_with("width");if(auto *live=buffer->GetScreenBuffer();live&&ScreenBuffer::IsExist(live)){value=width?live->GetWidth():live->GetHeight();return true;}Property p{buffer,width?"width_":"height_",-1};return RawRead(p,value);}
  auto p=Locate(object,path);Value raw;if(!RawRead(p,raw))return false;if(p.channel>=0){float a[4];if(raw.is_array()){if(p.channel>=static_cast<int>(raw.size()))return false;value=raw[p.channel];return true;}int count=raw.is_object()&&raw.contains("w")?4:raw.is_object()&&raw.contains("z")?3:2;if(!Array(raw,count,a)||p.channel>=count)return false;value=a[p.channel];return true;}value=raw;return true;
 }catch(const std::exception&){return false;}
}
bool WriteExtendedProperty(EmptyObject *object,const std::string &path,const Value &value){
 try{
  if(!object)return false;if(path=="object.active"){if(!value.is_boolean())return false;object->SetActive(value.get<bool>());return true;}
  auto p=Locate(object,path);if(p.channel<0)return RawWrite(p,value);Value raw;if(!RawRead(p,raw))return false;double n;if(!Number(value,n)||std::abs(n)>std::numeric_limits<float>::max())return false;
  if(raw.is_array()){if(p.channel>=static_cast<int>(raw.size()))return false;raw[p.channel]=value;return RawWrite(p,raw);}
  if(raw.is_object()){static const char* xyz[]={"x","y","z","w"};static const char* rgba[]={"r","g","b","a"};if(raw.contains(xyz[p.channel]))raw[xyz[p.channel]]=value;else if(raw.contains(rgba[p.channel]))raw[rgba[p.channel]]=value;else return false;return RawWrite(p,raw);}return false;
 }catch(const std::exception&){return false;}
}
}
