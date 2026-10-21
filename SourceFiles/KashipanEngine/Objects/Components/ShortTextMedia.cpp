#include "Scene/Components/GameEventRouter.h"
#include "ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/VideoSource.h"
#include "Objects/Components/GifSource.h"
#include "Objects/Components/Animator.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Shake.h"
#include "Objects/Components/ParticleSystem2D.h"
#include "Objects/Components/ParticleSystem3D.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Transform.h"
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <climits>

namespace KashipanEngine::ShortText {
namespace {
struct PersistentSnapshot { std::string sceneId; Value object; };
std::unordered_map<std::string,PersistentSnapshot> persistentSnapshots;
void RememberCreated(ProgramMemory *memory,const EmptyObject *object){if(memory&&object){auto id=object->GetObjectID().ToString();memory->extensionValues["lastSpawned"]=id;memory->extensionValues["last_created"]=id;}}
EmptyObject *UniqueObject(SceneContext *scene,const std::string &reference){if(!scene)return nullptr;UUID128 id(reference);if(id.IsValid())return scene->GetSceneObject(id);auto matches=scene->GetSceneObjects(reference);return matches.size()==1?matches.front():nullptr;}
template<class T> T *Select(EmptyObject *o,const Value &a) {
    if (!o) return nullptr;
    auto list=o->GetComponents<T>(); const int i=a.value("index",0);
    return i>=0 && static_cast<size_t>(i)<list.size() ? list[i] : nullptr;
}
bool Fail(std::string &e,const std::string &s) { e=s; return false; }
Vector3 Vec(const Value &a,const char *k,Vector3 fallback={0,0,0}) {
    if (!a.contains(k)) return fallback;
    const auto &v=a.at(k); if (!v.is_array()||v.size()!=3) throw std::runtime_error("vector must contain three numbers");
    Vector3 result{v[0].get<float>(),v[1].get<float>(),v[2].get<float>()};if(!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.z))throw std::runtime_error("vector must contain finite values");return result;
}
std::filesystem::path SavePath(const Value &a) {
    auto name=a.value("slot",std::string("default"));
    if(name.empty() || name.size()>80 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
        throw std::runtime_error("slot must be an alphanumeric filename");
    return std::filesystem::path("ShortTextSaves")/(name+".json");
}
void Remap(Value &v,const std::unordered_map<std::string,std::string>&ids) {
    if(v.is_string()){auto i=ids.find(v.get<std::string>());if(i!=ids.end())v=i->second;}
    else if(v.is_array()||v.is_object())for(auto &item:v)Remap(item,ids);
}
}
bool ExecuteMediaCommand(const std::string &cmd,const Value &a,CommandContext &c,std::string &error) {
    error.clear();
    static const std::unordered_set<std::string> known={"audio.play","audio.stop","audio.pause","audio.resume","audio.configure","video.play","video.stop","video.pause","video.resume","video.configure","gif.play","gif.stop","gif.pause","gif.resume","gif.configure","animator.play","animator.stop","animator.configure","keyframe.play","keyframe.stop","keyframe.configure","shake.play","shake.stop","shake.configure","particles.play","particles.stop","particles.clear","particles.configure","prefab.spawn","pool.acquire","pool.release","save.write","save.read","script.call","event.emit","object.persist","object.restore"};
    if(!known.contains(cmd))return false;
    try {
        if(!a.is_object())return Fail(error,"Command arguments must be an object");
        if(cmd.starts_with("audio.")) {
            auto *s=Select<AudioSource>(c.target,a);
            if(a.contains("sound") && c.target){s=nullptr;for(auto *candidate:c.target->GetComponents<AudioSource>())if(candidate->GetSoundName()==a.at("sound").get<std::string>()){if(s)return Fail(error,"Ambiguous sound source");s=candidate;}}
            if(!s)return Fail(error,"AudioSource not found");
            if(cmd=="audio.play")return s->Play()!=AudioManager::kInvalidPlayHandle || Fail(error,"Sound could not be played");
            if(cmd=="audio.stop"){s->Stop();if(c.memory&&c.target)c.memory->extensionValues["mediaStates"][c.target->GetObjectID().ToString()]["audio"]=false;return true;}
            if(cmd=="audio.pause")return s->Pause()||Fail(error,"Sound is not playing");
            if(cmd=="audio.resume")return s->Resume()||Fail(error,"Sound is not paused");
            if(cmd=="audio.configure"){
                if(a.contains("asset"))s->SetSoundName(a.at("asset").get<std::string>());
                if(a.contains("volume"))s->SetVolume(a.at("volume").get<float>());
                if(a.contains("pitch"))s->SetPitch(a.at("pitch").get<float>());
                if(a.contains("loop"))s->SetLoop(a.at("loop").get<bool>());
                if(a.contains("spatial"))s->SetEnableSpatialAudio(a.at("spatial").get<bool>());
                if(a.contains("minDistance"))s->SetMinDistance(a.at("minDistance").get<float>());
                if(a.contains("maxDistance"))s->SetMaxDistance(a.at("maxDistance").get<float>());
                return true;
            }
        }
        if(cmd.starts_with("video.")){
            auto *s=Select<VideoSource>(c.target,a);if(!s)return Fail(error,"VideoSource not found");
            if(cmd=="video.play")return s->Play()||Fail(error,"Video could not be played");
            if(cmd=="video.stop"){s->Stop();if(c.memory&&c.target)c.memory->extensionValues["mediaStates"][c.target->GetObjectID().ToString()]["video"]=false;return true;}
            if(cmd=="video.pause")return s->Pause()||Fail(error,"Video is not playing");
            if(cmd=="video.resume")return s->Resume()||Fail(error,"Video is not paused");
            if(cmd=="video.configure"){if(a.contains("asset"))s->SetVideoAssetPath(a.at("asset").get<std::string>());if(a.contains("loop"))s->SetLoop(a.at("loop").get<bool>());if(a.contains("volume"))s->SetVolume(a.at("volume").get<float>());return true;}
        }
        if(cmd.starts_with("gif.")){
            auto *s=Select<GifSource>(c.target,a);if(!s)return Fail(error,"GifSource not found");
            if(cmd=="gif.play")return s->Play()||Fail(error,"Gif could not be played");
            if(cmd=="gif.stop"){s->Stop();if(c.memory&&c.target)c.memory->extensionValues["mediaStates"][c.target->GetObjectID().ToString()]["gif"]=false;return true;}
            if(cmd=="gif.pause")return s->Pause()||Fail(error,"Gif is not playing");
            if(cmd=="gif.resume")return s->Resume()||Fail(error,"Gif is not paused");
            if(cmd=="gif.configure"){if(a.contains("asset"))s->SetGifAssetPath(a.at("asset").get<std::string>());if(a.contains("loop"))s->SetLoop(a.at("loop").get<bool>());return true;}
        }
        if(cmd.starts_with("animator.")){
            auto *s=Select<Animator>(c.target,a);if(!s)return Fail(error,"Animator not found");
            if(cmd=="animator.play"){if(a.contains("clip"))s->SetClipName(a.at("clip").get<std::string>());s->Play();return true;}
            if(cmd=="animator.stop"){s->Stop();if(c.memory&&c.target)c.memory->extensionValues["mediaStates"][c.target->GetObjectID().ToString()]["animator"]=false;return true;}
            if(cmd=="animator.configure"){if(a.contains("speed"))s->SetPlaybackSpeed(a.at("speed").get<float>());if(a.contains("loop"))s->SetLoop(a.at("loop").get<bool>());if(a.contains("time"))s->SetPlaybackTime(a.at("time").get<float>());return true;}
        }
        if(cmd.starts_with("keyframe.")){
            auto *s=Select<KeyFrameAnimator>(c.target,a);if(!s)return Fail(error,"KeyFrameAnimator not found");const auto name=a.value("name",std::string{});
            if(!s->HasAnimation(name))return Fail(error,"Named keyframe animation not found");
            if(cmd=="keyframe.play")return s->Play(name);
            if(cmd=="keyframe.stop")return s->Stop(name);
            if(cmd=="keyframe.configure"){if(a.contains("speed"))s->SetPlaybackSpeed(name,a.at("speed").get<float>());if(a.contains("loop"))s->SetLoop(name,a.at("loop").get<bool>());if(a.contains("time"))s->SetCurrentTime(name,a.at("time").get<float>());return true;}
        }
        if(cmd.starts_with("shake.")){
            auto *s=Select<Shake>(c.target,a);if(!s)return Fail(error,"Shake not found");
            if(cmd=="shake.play"){s->Play(a.value("duration",Shake::kUseConfiguredDuration));return true;}
            if(cmd=="shake.stop"){s->Stop();return true;}
            if(cmd=="shake.configure"){if(a.contains("amplitude"))s->SetPositionAmplitude(Vec(a,"amplitude"));if(a.contains("speed"))s->SetPositionSpeed(Vec(a,"speed"));if(a.contains("rotationAmplitude"))s->SetRotationAmplitude(Vec(a,"rotationAmplitude"));return true;}
        }
        if(cmd.starts_with("particles.")){
            ParticleSystemBase *s=Select<ParticleSystem2D>(c.target,a);if(!s)s=Select<ParticleSystem3D>(c.target,a);if(!s)return Fail(error,"ParticleSystem not found");
            if(cmd=="particles.play"){s->Play();return true;}
            if(cmd=="particles.stop"){s->Stop();return true;}
            if(cmd=="particles.clear"){s->Clear();return true;}
            if(cmd=="particles.configure"){if(a.contains("rate"))s->SetEmissionRate(a.at("rate").get<float>());if(a.contains("count"))s->SetTotalSpawnCount(a.at("count").get<int>());return true;}
        }
        if(cmd=="script.call"){
            auto *s=Select<ScriptComponent>(c.target,a);if(!s)return Fail(error,"ScriptComponent not found");return s->InvokeMethod(a.at("method").get<std::string>())||Fail(error,"Script method could not be invoked");
        }
        if(cmd=="event.emit"){
            if(!c.scene)return Fail(error,"Scene unavailable");const auto name=a.at("name").get<std::string>();if(name.empty())return Fail(error,"Event name is empty");
            // Compatibility: legacy watchers read the __shortTextEvent counter; a scene event definition of the same name also
            // receives exactly one routed delivery (the router never touches the counter, so nothing is delivered twice).
            // The routed payload is validated first so a rejected send leaves the counter unchanged.
            if(auto *router=GameEventRouter::Find(c.scene);router&&router->FindDefinition(name)){std::string routeError;if(!router->Emit(name,a.value("payload",Value::object()),c.owner?c.owner->GetObjectID().ToString():std::string{},GameEvents::TargetMode::Broadcast,"",&routeError))return Fail(error,routeError);}
            auto *value=c.scene->GetSceneVariable("__shortTextEvent."+name);int serial=value&&value->IsType<int>()?value->AnyCast<int>():0;
            c.scene->AddSceneVariable<int>("__shortTextEvent."+name,serial>=INT_MAX||serial<0?1:serial+1);
            return true;
        }
        if(cmd=="save.write"||cmd=="save.read"){
            if(!c.scene)return Fail(error,"Scene unavailable");auto p=SavePath(a);if(cmd=="save.write"){return c.scene->SaveGlobalSceneVariables(p.string())||Fail(error,"Save failed");}
            return c.scene->LoadGlobalSceneVariables(p.string())||Fail(error,"Load failed");
        }
        if(cmd=="object.persist"||cmd=="object.restore"){
            if(!c.scene)return Fail(error,"Scene unavailable");const auto key="__shortTextObject."+a.value("key",std::string("default"));
            if(cmd=="object.persist"){if(!c.target)return Fail(error,"Target unavailable");if(auto *transform=c.target->GetComponent<Transform>();transform&&transform->GetParentObject())return Fail(error,"Persistent snapshots require a root object");if(persistentSnapshots.size()>=64 && !persistentSnapshots.contains(key))return Fail(error,"Persistent object limit reached");auto snapshot=c.scene->SaveObjectToJson(c.target);c.scene->RemoveGlobalSceneVariable(key);c.scene->AddGlobalSceneVariable<std::string>(key,snapshot.dump());persistentSnapshots[key]={c.scene->GetSceneID().ToString(),snapshot};return true;}
            if(c.scene->GetSceneObjects().size()>=4096)return Fail(error,"Scene object limit reached");auto *v=c.scene->GetGlobalSceneVariable(key);if(!v||!v->IsType<std::string>())return Fail(error,"No persistent snapshot");auto j=Value::parse(v->AnyCast<std::string>());const auto newId=UUID128(true).ToString();Remap(j,{{j.value("objectID",std::string{}),newId}});j["objectID"]=newId;auto *o=c.scene->CreateObjectFromJson(j);if(!o)return Fail(error,"Restore failed");if(auto *t=o->GetComponent<Transform>())t->SetParentObject(nullptr);RememberCreated(c.memory,o);return true;
        }
        if(cmd=="prefab.spawn"){
            if(!c.scene)return Fail(error,"Scene unavailable");std::ifstream input(a.at("path").get<std::string>());if(!input)return Fail(error,"Prefab file unavailable");Value j;input>>j;
            Value nodes=j.contains("objects")?j.at("objects"):Value::array({Value{{"parentIndex",-1},{"object",j}}});
            if(!nodes.is_array()||nodes.empty()||nodes.size()>256)return Fail(error,"Prefab needs 1 to 256 objects");
            if(c.scene->GetSceneObjects().size()+nodes.size()>4096)return Fail(error,"Scene object limit reached");
            EmptyObject *parent=nullptr;if(a.contains("parent")){parent=UniqueObject(c.scene,a.at("parent").get<std::string>());if(!parent)return Fail(error,"Prefab parent missing or ambiguous");}
            std::unordered_map<std::string,std::string>ids;std::vector<std::string> fresh;
            for(size_t i=0;i<nodes.size();++i){const auto &n=nodes[i];if(!n.is_object()||!n.contains("object")||!n.at("object").is_object()||(n.value("parentIndex",-1)<-1 || n.value("parentIndex",-1)>=static_cast<int>(i)))return Fail(error,"Invalid prefab hierarchy");auto id=UUID128(true).ToString();ids[n["object"].value("objectID",std::string{})]=id;fresh.push_back(id);}
            if(a.contains("position"))Vec(a,"position");if(a.contains("rotation"))Vec(a,"rotation");if(a.contains("scale"))Vec(a,"scale");
            if(a.contains("initial")&&!a.at("initial").is_object())return Fail(error,"Initial properties must be an object");
            std::vector<EmptyObject*> created;
            for(size_t i=0;i<nodes.size();++i){auto data=nodes[i]["object"];Remap(data,ids);data["objectID"]=fresh[i];auto *o=c.scene->CreateObjectFromJson(data);if(!o){for(auto *p:created)c.scene->DeleteObject(p);return Fail(error,"Prefab creation failed");}created.push_back(o);}
            for(size_t i=0;i<nodes.size();++i)if(auto *t=created[i]->GetComponent<Transform>()){int p=nodes[i].value("parentIndex",-1);t->SetParentObject(p>=0?created[p]:nullptr);}
            auto *root=created.front();if(auto *t=root->GetComponent<Transform>()){if(a.contains("position"))t->SetTranslate(Vec(a,"position"));if(a.contains("rotation"))t->SetRotate(Vec(a,"rotation"));if(a.contains("scale"))t->SetScale(Vec(a,"scale"));if(a.contains("parent"))t->SetParentObject(parent);}
            if(a.contains("initial"))for(auto &v:a.at("initial").items())if(!WriteExtendedProperty(root,v.key(),v.value())){for(auto *p:created)c.scene->DeleteObject(p);return Fail(error,"Initial property could not be assigned");}
            RememberCreated(c.memory,root);return true;
        }
        if(cmd=="pool.acquire"||cmd=="pool.release"){
            if(!c.scene||!c.memory||!c.target)return Fail(error,"Pool requires scene, target and program memory");auto key=a.value("pool",std::string("default"));auto &list=c.memory->extensionValues["pools"][key];if(!list.is_array())list=Value::array();
            if(cmd=="pool.release"){auto id=c.target->GetObjectID().ToString();bool owned=false;for(const auto&x:list)if(x==id)owned=true;if(!owned)return Fail(error,"Target does not belong to this pool");c.target->SetActive(false);return true;}
            for(auto it=list.begin();it!=list.end();)if(!c.scene->GetSceneObject(UUID128(it->get<std::string>())))it=list.erase(it);else ++it;
            EmptyObject *result=nullptr;for(const auto&id:list){auto *o=c.scene->GetSceneObject(UUID128(id.get<std::string>()));if(o&&!o->IsActive()){result=o;break;}}
            if(!result){if(c.scene->GetSceneObjects().size()>=4096)return Fail(error,"Scene object limit reached");int limit=a.value("limit",16);if(limit<1||limit>256||list.size()>=static_cast<size_t>(limit))return Fail(error,"Pool capacity reached");result=c.scene->CloneObject(c.target,a.value("name",std::string("PooledObject")),false);if(!result)return Fail(error,"Pool clone failed");list.push_back(result->GetObjectID().ToString());}
            if(auto *t=result->GetComponent<Transform>();t&&a.contains("position"))t->SetTranslate(Vec(a,"position"));result->SetActive(true);RememberCreated(c.memory,result);return true;
        }
        return Fail(error,"Unknown media command: "+cmd);
    }catch(const std::exception &e){return Fail(error,e.what());}
}
void ReadMediaValues(const EmptyObject *owner,SceneContext *scene,ProgramState &state,ProgramMemory *memory,double dt){
    (void)dt;
    if(scene){const auto sceneId=scene->GetSceneID().ToString();for(auto &[key,entry]:persistentSnapshots)if(entry.sceneId!=sceneId){if(scene->GetSceneObjects().size()>=4096)continue;auto snapshot=entry.object;const auto oldId=snapshot.value("objectID",std::string{});auto newId=UUID128(true).ToString();Remap(snapshot,{{oldId,newId}});snapshot["objectID"]=newId;if(auto *created=scene->CreateObjectFromJson(snapshot)){if(auto *t=created->GetComponent<Transform>())t->SetParentObject(nullptr);entry.sceneId=sceneId;entry.object=snapshot;scene->RemoveGlobalSceneVariable(key);scene->AddGlobalSceneVariable<std::string>(key,snapshot.dump());}}else{if(auto *live=scene->GetSceneObject(UUID128(entry.object.value("objectID",std::string{}))))entry.object=scene->SaveObjectToJson(live);}}
    if(!owner)return;auto &e=state.environment;
    auto edge=[&](const std::string &key,bool playing,bool paused=false){bool finished=false;if(memory){auto &old=memory->extensionValues["mediaStates"][owner->GetObjectID().ToString()][key];if(old.is_boolean())finished=old.get<bool>()&&!playing&&!paused;old=playing||paused;}e[key+".finished"]=finished;e[key]["finished"]=finished;};
    if(auto *s=owner->GetComponent<AudioSource>()){edge("audio",s->IsPlaying(),s->IsPaused());e["audio.playing"]=s->IsPlaying();e["audio.paused"]=s->IsPaused();e["audio.volume"]=s->GetVolume();e["audio"]["playing"]=s->IsPlaying();e["audio"]["paused"]=s->IsPaused();e["audio"]["volume"]=s->GetVolume();}
    if(auto *s=owner->GetComponent<Animator>()){edge("animator",s->IsPlaying());e["animator.playing"]=s->IsPlaying();e["animator.time"]=s->GetPlaybackTime();e["animator"]["playing"]=s->IsPlaying();e["animator"]["time"]=s->GetPlaybackTime();}
    if(auto *s=owner->GetComponent<KeyFrameAnimator>()){
        auto data=owner->SaveComponentToJson(s);const auto &custom=data["data"]["customData"];
        if(custom.contains("animations")&&custom["animations"].is_array())for(const auto &entry:custom["animations"]){auto name=entry.value("name",std::string{});e["keyframe"][name]["playing"]=s->IsPlaying(name);e["keyframe"][name]["time"]=s->GetAnimationTime(name);const auto end=s->GetDuration(name);e["keyframe"][name]["finished"]=!s->IsPlaying(name)&&end>0&&s->GetAnimationTime(name)>=end;}
    }
    if(auto *s=owner->GetComponent<VideoSource>()){edge("video",s->IsPlaying(),s->IsPaused());e["video.playing"]=s->IsPlaying();e["video.paused"]=s->IsPaused();}
    if(auto *s=owner->GetComponent<GifSource>()){edge("gif",s->IsPlaying(),s->IsPaused());e["gif.playing"]=s->IsPlaying();e["gif.paused"]=s->IsPaused();}
    if(auto *s=owner->GetComponent<Shake>())e["shake.playing"]=s->IsPlaying();
}
}
