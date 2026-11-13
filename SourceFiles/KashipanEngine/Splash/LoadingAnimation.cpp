#include "LoadingAnimation.h"
#include "Utilities/Conversion/ConvertString.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace KashipanEngine::LoadingAnimation {
namespace {
JSON Data(const JSON &c) { auto d=c.value("data", JSON::object()); return d.value("customData",d); }
bool Enabled(const JSON &c) { return c.value("data",JSON::object()).value("isActive",true); }
float Number(const JSON &j,const char *key,float fallback=0) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    const float v=j[key].get<float>(); return std::isfinite(v) ? v : fallback;
}
float Channel(const JSON &j,const char *key,const char *axis,float fallback) {
    return j.contains(key) && j[key].is_object() ? Number(j[key],axis,fallback) : fallback;
}
bool Supported(const std::string &type,const std::string &p,int ch) {
    return (type=="Transform" && ((p=="translate_" || p=="scale_") && ch>=0 && ch<2 || p=="rotate_" && ch==2)) ||
        (type=="SpriteRenderer" && ((p=="instanceColor_" && ch>=0 && ch<4) ||
        ((p=="instanceUvTranslate_" || p=="instanceUvScale_") && ch>=0 && ch<2))) ||
        (type=="LoadingAnimationSection" && p=="opacity_");
}
float *Property(Node &n,const Track &t) {
    if(t.property=="translate_") return t.channel ? &n.y : &n.x;
    if(t.property=="scale_") return t.channel ? &n.sy : &n.sx;
    if(t.property=="rotate_") return &n.rotation;
    if(t.property=="instanceColor_") { if(t.channel==0)return &n.r; if(t.channel==1)return &n.g; if(t.channel==2)return &n.b; return &n.a; }
    if(t.property=="instanceUvTranslate_")return t.channel ? &n.v : &n.u;
    if(t.property=="instanceUvScale_")return t.channel ? &n.vs : &n.us;
    return &n.opacity;
}
void Apply(std::vector<Node> &nodes,const Track &t,double seconds,bool endpoint=false) {
    float time=static_cast<float>(seconds)*t.speed+t.offset;
    const auto duration=t.animation.GetDuration();
    if(t.speed<0)time+=duration;
    if(t.loop && duration>0) {
        const float original=time; time=std::fmod(time,duration); if(time<0)time+=duration;
        if(endpoint && t.speed>0 && original>0 && std::abs(time)<.00001f)time=duration;
    }
    *Property(nodes[t.target],t)=t.animation.Evaluate(time)*t.scale+t.bias;
}
}
Document Compile(const JSON &scene,const MaterialResolver &material) {
    Document out; out.package=scene;
    try {
        if(!scene.contains("sceneObjects") || !scene["sceneObjects"].is_array()) { out.errors.push_back("sceneObjectsがありません。"); return out; }
        auto &objects=out.package["sceneObjects"];
        std::unordered_map<std::string,int> ids;
        std::vector<std::string> parents;
        std::vector<int> markers;
        std::vector<JSON> sectionConfigs;
        std::array<int,4> roots{-1,-1,-1,-1};
        std::array<bool,4> duplicate{};
        std::array<bool,3> automatic{};
        std::vector<std::pair<int,std::string>> nodeIssues;
        auto issue=[&](int node,const std::string &text) { nodeIssues.emplace_back(node,text); };
        for(auto &obj:objects) {
            Node n; n.id=obj.value("objectID",std::string{}); n.name=obj.value("name",n.id); n.active=obj.value("isActive",true);
            if(n.id.empty() || ids.contains(n.id)) { out.errors.push_back("オブジェクトIDが空または重複しています。"); return out; }
            ids[n.id]=static_cast<int>(out.nodes.size());
            std::string parent, mesh; int marker=-1; JSON config;
            for(auto &c:obj["components"]) {
                if(!Enabled(c))continue;
                const auto type=c.value("type",std::string{}); const auto d=Data(c);
                if(type=="Transform") {
                    n.x=Channel(d,"translate","x",0); n.y=Channel(d,"translate","y",0);
                    n.sx=Channel(d,"scale","x",1); n.sy=Channel(d,"scale","y",1);
                    const auto z=Channel(d,"rotate","z",0),w=Channel(d,"rotate","w",1);
                    n.rotation=std::atan2(2*w*z,1-2*z*z);
                    if(std::abs(Channel(d,"rotate","x",0))+std::abs(Channel(d,"rotate","y",0))>.0001f) issue(static_cast<int>(out.nodes.size()),n.name+": 3D回転は使用できません。");
                    parent=d.value("parent",std::string{});
                } else if(type=="LoadingAnimationSection") {
                    marker=d.value("section",-1);
                    if(marker>=0 && marker<4) {
                        config=d;
                        n.opacity=Number(d,"opacity",1);
                    } else out.warnings.push_back(n.name+": 不明な区間を無視しました。");
                } else if(type=="SpriteRenderer") {
                    if(n.sprite) { issue(static_cast<int>(out.nodes.size()),n.name+": SpriteRendererは1個にしてください。"); continue; }
                    n.sprite=true; n.priority=d.value("renderPriority",0);
                    n.anchorX=Channel(d,"anchor","x",.5f); n.anchorY=Channel(d,"anchor","y",.5f);
                    n.r=Channel(d,"instanceColor","x",1); n.g=Channel(d,"instanceColor","y",1); n.b=Channel(d,"instanceColor","z",1); n.a=Channel(d,"instanceColor","w",1);
                    n.u=Channel(d,"instanceUvTranslate","x",0); n.v=Channel(d,"instanceUvTranslate","y",0);
                    n.us=Channel(d,"instanceUvScale","x",1); n.vs=Channel(d,"instanceUvScale","y",1);
                    if(Number(d,"instanceUvRotation")!=0)issue(static_cast<int>(out.nodes.size()),n.name+": UV回転は使用できません。");
                    JSON resolved=c.value("startupMaterial",JSON::object());
                    if(material && !material(d.value("materialName",std::string("Default")),resolved)) issue(static_cast<int>(out.nodes.size()),n.name+": マテリアルを解決できません。");
                    if(resolved.empty()) issue(static_cast<int>(out.nodes.size()),n.name+": 起動用マテリアルがありません。");
                    else {
                        c["startupMaterial"]=resolved; n.image=resolved.value("image",std::string{});
                        const int blend=d.value("instanceColorBlendMode",0);
                        n.colorBlend=blend; n.uvCombine=d.value("instanceUvCombineMode",0);
                        n.materialR=Number(resolved,"r",1); n.materialG=Number(resolved,"g",1); n.materialB=Number(resolved,"b",1); n.materialA=Number(resolved,"a",1);
                        n.materialU=Number(resolved,"u"); n.materialV=Number(resolved,"v"); n.materialUS=Number(resolved,"us",1); n.materialVS=Number(resolved,"vs",1);
                        if(blend>1)issue(static_cast<int>(out.nodes.size()),n.name+": 色合成はOverride/Multiplyのみ対応します。");
                        if(Number(resolved,"uvRotation")!=0)issue(static_cast<int>(out.nodes.size()),n.name+": マテリアルのUV回転は使用できません。");
                        const auto pipeline=d.value("pipelineName",std::string{});
                        if(!pipeline.empty() && pipeline!="Object2D" && pipeline!="Object2D.DoubleSidedCulling.BlendNormal")issue(static_cast<int>(out.nodes.size()),n.name+": 起動用描画は標準Object2Dパイプラインのみ対応します。");
                    }
                } else if(type=="MeshFilter") mesh=d.value("meshAssetPath",std::string{});
                else if(type!="KeyFrameAnimator") out.warnings.push_back(n.name+": "+type+"は起動中に実行・描画しません。");
            }
            if(n.sprite && !mesh.empty() && mesh!="PrimitiveMesh-Rect2D") issue(static_cast<int>(out.nodes.size()),n.name+": 起動用スプライトはRect2Dメッシュを使用してください。");
            out.nodes.push_back(n); parents.push_back(parent); markers.push_back(marker); sectionConfigs.push_back(config);
        }
        for(size_t i=0;i<out.nodes.size();++i) {
            auto it=ids.find(parents[i]); if(it!=ids.end())out.nodes[i].parent=it->second;
            else if(!parents[i].empty())issue(static_cast<int>(i),out.nodes[i].name+": 親が見つかりません。");
        }
        for(size_t i=0;i<out.nodes.size();++i) {
            auto &n=out.nodes[i]; std::unordered_set<int> visited;
            for(int p=static_cast<int>(i);p>=0;p=out.nodes[p].parent) {
                if(!visited.insert(p).second) { issue(static_cast<int>(i),n.name+": 親子関係が循環しています。"); break; }
                n.active=n.active && out.nodes[p].active;
                if(n.section<0 && markers[p]>=0 && markers[p]<4)n.section=markers[p];
            }
        }
        for(size_t i=0;i<out.nodes.size();++i) {
            const int section=markers[i];
            if(!out.nodes[i].active || section<0 || section>=4)continue;
            if(roots[section]>=0)duplicate[section]=true;
            roots[section]=static_cast<int>(i);
            if(section<3) {
                const auto &d=sectionConfigs[i]; automatic[section]=d.value("endMode",1)==1;
                auto &timing=out.timing[section];
                timing.seconds=automatic[section] ? -1 : std::max(0.f,Number(d,"seconds"));
                timing.cycleEnd=d.value("completion",1)!=0;
                timing.source=automatic[section] ? "自動 (有効なアニメーションなし)" : "秒数指定";
            }
        }
        for(size_t i=0;i<objects.size();++i) for(auto &c:objects[i]["components"]) {
            if(c.value("type",std::string{})!="KeyFrameAnimator" || !Enabled(c) || !out.nodes[i].active || out.nodes[i].section<0)continue;
            auto d=Data(c); if(!d.contains("animations"))continue;
            for(auto &a:d["animations"]) {
                if(!a.value("playOnStart",false))continue;
                KeyframeAnimation animation; JSON data=a.value("embeddedAnimation",JSON());
                if(data.is_null()) { try { std::ifstream f(Utf8StringToPath(a.value("jsonPath",std::string{}))); if(f) f>>data; } catch(...) { data=JSON(); } }
                if(!animation.LoadFromJson(data) || animation.GetDuration()<=0) { out.warnings.push_back(out.nodes[i].name+": 無効なキーフレームを無視しました。"); continue; }
                a["embeddedAnimation"]=animation.SaveToJson(); a["jsonPath"]="";
                const float speed=Number(a,"playbackSpeed",1),offset=Number(a,"timeOffset");
                if(speed==0) { out.warnings.push_back(out.nodes[i].name+": 再生速度0を無視しました。"); continue; }
                bool effective=false;
                for(const auto &b:a.value("bindings",JSON::array())) {
                    Track t; t.owner=static_cast<int>(i); t.section=out.nodes[i].section;
                    const auto target=b.value("targetObjectID",std::string{}); auto it=ids.find(target);
                    t.target=target.empty() ? static_cast<int>(i) : it==ids.end() ? -1 : it->second;
                    t.property=b.value("parameterName",std::string{}); t.channel=b.value("channel",0);
                    if(t.target<0 || !out.nodes[t.target].active || (out.nodes[t.target].section!=t.section && out.nodes[t.target].section!=3)) { issue(static_cast<int>(i),out.nodes[i].name+": アニメーション先は同区間または共通区間にしてください。"); continue; }
                    if(b.value("componentIndex",0)!=0 || b.value("isScriptVariable",false) || !Supported(b.value("componentType",std::string{}),t.property,t.channel)) { issue(static_cast<int>(i),out.nodes[i].name+": 未対応のアニメーション対象: "+t.property); continue; }
                    const auto type=b.value("componentType",std::string{});
                    bool targetExists=false;
                    for(const auto &component:objects[t.target]["components"])if(Enabled(component) && component.value("type",std::string{})==type)targetExists=true;
                    if(!targetExists) { issue(static_cast<int>(i),out.nodes[i].name+": 適用先のコンポーネントがありません。"); continue; }
                    t.animation=animation; t.speed=speed; t.offset=offset; t.loop=a.value("loop",true); t.scale=Number(a,"valueScale",1); t.bias=Number(a,"valueOffset"); t.name=a.value("name",std::string{});
                    out.tracks.push_back(t); effective=true;
                }
                (void)effective;
            }
            if(c["data"].contains("customData"))c["data"]["customData"]=d; else c["data"]=d;
        }
        for(int s=0;s<3;++s) {
            auto &timing=out.timing[s];
            if(automatic[s]) {
                timing.seconds=0;
                for(const auto &t:out.tracks) if(t.section==s) {
                    const float span=t.loop ? t.animation.GetDuration() : (t.speed>0 ? t.animation.GetDuration()-t.offset : t.animation.GetDuration()+t.offset);
                    const double length=std::max(0.0,double(span/std::abs(t.speed)));
                    if(length>timing.seconds) { timing.seconds=length; timing.source=out.nodes[t.owner].name+" / "+t.name; }
                }
            }
            if(duplicate[s]) { timing.seconds=0; out.warnings.push_back("同じ区間のルートが複数あります。この区間をスキップします。"); }
            if(!std::isfinite(timing.seconds) || timing.seconds<=0) { timing.seconds=0; out.warnings.push_back(std::to_string(s)+": 未指定・無効な区間をスキップします。"); }
        }
        if(duplicate[3])out.errors.push_back("共通区間のルートは1個にしてください。");
        for(const auto &[index,text]:nodeIssues) {
            const auto &n=out.nodes[index];
            if(n.active && n.section>=0 && (n.section==3 || out.timing[n.section].seconds>0))out.errors.push_back(text);
            else out.warnings.push_back("対象外の区間: "+text);
        }
        for(size_t i=0;i<out.nodes.size();++i) {
            auto &n=out.nodes[i];
            if(!n.active || n.section<0 || (n.section<3 && out.timing[n.section].seconds<=0)) {
                n.sprite=false; n.image.clear();
                auto &components=objects[i]["components"];
                components.erase(std::remove_if(components.begin(),components.end(),[](const JSON &c) {
                    const auto t=c.value("type",std::string{}); return t=="SpriteRenderer" || t=="KeyFrameAnimator";
                }),components.end());
            }
        }
        // Export only render/animation metadata; do not retain script bodies or other scene state.
        for(auto &object:objects) {
            auto &components=object["components"];
            components.erase(std::remove_if(components.begin(),components.end(),[](const JSON &c) {
                const auto t=c.value("type",std::string{});
                return t!="Transform" && t!="SpriteRenderer" && t!="KeyFrameAnimator" && t!="LoadingAnimationSection";
            }),components.end());
        }
        out.package=JSON{{"startupAnimationVersion",1},{"sceneObjects",objects}};
    }catch(const std::exception &e) { out.errors.push_back(std::string("ロード画面の形式が不正です: ")+e.what()); }
    return out;
}
Frame Document::Sample(double elapsed,double readyAt) const {
    Frame f; f.elapsed=std::max(0.0,elapsed); const double intro=timing[0].seconds;
    if(f.elapsed<intro) { f.section=Section::Intro; f.local=f.elapsed; return f; }
    if(readyAt<0 || !std::isfinite(readyAt)) { f.section=Section::Waiting; f.local=timing[1].seconds>0 ? std::fmod(f.elapsed-intro,timing[1].seconds) : 0; return f; }
    double waitEnd=std::max(intro,readyAt);
    if(timing[1].seconds>0 && timing[1].cycleEnd && readyAt>intro)waitEnd=intro+std::ceil((readyAt-intro)/timing[1].seconds)*timing[1].seconds;
    f.waitingEnd=waitEnd;
    if(f.elapsed<waitEnd) { f.section=Section::Waiting; f.local=timing[1].seconds>0 ? std::fmod(f.elapsed-intro,timing[1].seconds) : 0; return f; }
    f.local=f.elapsed-waitEnd;
    f.finished=f.local>=timing[2].seconds;
    f.section=f.finished ? Section::Finished : Section::Outro; return f;
}
std::vector<Node> Document::Evaluate(const Frame &f) const {
    auto result=nodes;
    // Completed phases leave their last value on common objects. Current phase wins over common tracks.
    for(int s=0;s<3;++s) if(s<static_cast<int>(f.section) && timing[s].seconds>0) for(const auto &t:tracks)
        if(t.section==s && nodes[t.target].section==3)Apply(result,t,s==1 ? std::max(0.0,f.waitingEnd-timing[0].seconds) : timing[s].seconds,true);
    for(const auto &t:tracks)if(t.section==3)Apply(result,t,f.elapsed);
    for(const auto &t:tracks)if(t.section>=0 && t.section<3 && t.section==static_cast<int>(f.section) && timing[t.section].seconds>0)Apply(result,t,f.local);
    for(auto &n:result)if(n.section!=3 && (n.section!=static_cast<int>(f.section) || n.section<0 || n.section>=3 || timing[n.section].seconds<=0))n.active=false;
    return result;
}
bool Export(Document &d,const std::filesystem::path &folder,std::string &error) {
    if(!d.errors.empty()) { error="検証エラーを修正してください。"; return false; }
    try {
        std::filesystem::create_directories(folder/"images");
        auto package=d.package; std::unordered_map<std::string,std::string> images;
        for(auto &o:package["sceneObjects"])for(auto &c:o["components"])if(c.contains("startupMaterial")) {
            auto &image=c["startupMaterial"]["image"]; const auto source=image.get<std::string>(); if(source.empty())continue;
            if(!images.contains(source)) {
                auto path=Utf8StringToPath(source); const auto ext=path.extension().wstring();
                if(ext!=L".png" && ext!=L".jpg" && ext!=L".jpeg" && ext!=L".bmp" && ext!=L".tif" && ext!=L".tiff")throw std::runtime_error("起動用画像はPNG/JPEG/BMP/TIFFを使用してください。");
                std::ifstream input(path,std::ios::binary); if(!input)throw std::runtime_error("起動用画像を読み込めません。");
                uint64_t fingerprint=14695981039346656037ULL; char byte;
                while(input.get(byte)) { fingerprint^=static_cast<unsigned char>(byte); fingerprint*=1099511628211ULL; }
                const auto dest=std::filesystem::path("images")/(std::to_string(fingerprint)+PathToUtf8String(path.extension()));
                std::filesystem::copy_file(path,folder/dest,std::filesystem::copy_options::overwrite_existing); images[source]=PathToUtf8String(dest);
            }
            image=images[source];
        }
        const auto temporary=folder/"animation.tmp"; std::ofstream f(temporary,std::ios::binary); f<<package.dump(2); f.close(); if(!f)throw std::runtime_error("書き込みに失敗しました。");
        // Atomic replacement on Windows; a failed export keeps the previous active JSON.
        if(!MoveFileExW(temporary.c_str(),(folder/"animation.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("起動アニメーションの切り替えに失敗しました。");
        return true;
    }catch(const std::exception &e) { error=e.what(); return false; }
}
Document Load(const std::filesystem::path &file) {
    Document d;
    try {
        std::ifstream f(file,std::ios::binary); JSON j; f>>j;
        if(j.value("startupAnimationVersion",0)!=1)throw std::runtime_error("起動用データのバージョンが不明です。");
        d=Compile(j); d.folder=file.parent_path();
        for(const auto &n:d.nodes)if(!n.image.empty()) {
            const auto path=Utf8StringToPath(n.image); if(path.is_absolute())throw std::runtime_error("画像パスはパッケージ内の相対パスにしてください。");
            for(const auto &part:path)if(part==L"..")throw std::runtime_error("画像パスがパッケージの外を参照しています。");
            if(!std::filesystem::is_regular_file(d.folder/path))throw std::runtime_error("起動用画像が見つかりません。");
        }
    }catch(const std::exception &e) { d.errors.push_back(e.what()); }
    return d;
}
}
