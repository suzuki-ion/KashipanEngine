#include "Splash/LoadingAnimation.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <windows.h>
using namespace KashipanEngine;
using namespace KashipanEngine::LoadingAnimation;
namespace {
int checks=0;
void Check(bool v,const char *message) { ++checks; if(!v)throw std::runtime_error(message); }
JSON Component(const char *type,JSON data) { return JSON{{"type",type},{"data",{{"isActive",true},{"customData",data}}}}; }
JSON Object(const char *id,const char *parent="") {
    return JSON{{"objectID",id},{"name",id},{"isActive",true},{"components",JSON::array({Component("Transform",{{"parent",parent}})})}};
}
void SectionRoot(JSON &scene,const char *id,int section,int mode,float seconds) {
    auto o=Object(id); o["components"].push_back(Component("LoadingAnimationSection",{{"section",section},{"endMode",mode},{"seconds",seconds}})); scene["sceneObjects"].push_back(o);
}
void Animate(JSON &object,const char *target,float duration,bool loop=false,float speed=1,float offset=0) {
    JSON clip={{"keyframes",JSON::array({{{"time",0},{"value",0}},{{"time",duration},{"value",100},{"ease","EaseOutBack"}}})}};
    JSON entry={{"name","move"},{"embeddedAnimation",clip},{"playOnStart",true},{"loop",loop},{"playbackSpeed",speed},{"timeOffset",offset},
        {"bindings",JSON::array({{{"componentType","Transform"},{"parameterName","translate_"},{"channel",0},{"targetObjectID",target}}})}};
    object["components"].push_back(Component("KeyFrameAnimator",{{"animations",JSON::array({entry})}}));
}
JSON Scene() { return JSON{{"sceneObjects",JSON::array()}}; }
}
int main() {
    try {
        auto scene=Scene(); SectionRoot(scene,"common",3,0,0); auto sprite=Object("sprite","common");
        sprite["components"].push_back(Component("SpriteRenderer",JSON::object()));
        scene["sceneObjects"].push_back(sprite);
        const MaterialResolver material=[](const std::string&,JSON &j){j={{"image",""}};return true;};
        auto d=Compile(scene,material);
        Check(d.errors.empty(),"common-only scene valid"); Check(!d.Sample(100,-1).finished,"no valid section still waits for load");
        Check(d.Sample(100,99).finished,"all missing sections finish when ready");
        Check(d.Evaluate(d.Sample(1,-1))[1].active,"common static sprite remains visible");
        auto inactiveScene=scene; SectionRoot(inactiveScene,"inactive common",3,0,0); inactiveScene["sceneObjects"].back()["isActive"]=false;
        Check(Compile(inactiveScene,material).errors.empty(),"inactive common root is ignored");
        SectionRoot(scene,"intro",0,0,-4); SectionRoot(scene,"waiting",1,0,0); SectionRoot(scene,"outro",2,0,1);
        d=Compile(scene,material); Check(d.errors.empty(),"invalid seconds not export error");
        Check(d.timing[0].seconds==0 && d.timing[1].seconds==0,"negative and zero duration skipped");
        Check(d.Sample(.5,-1).section==Section::Waiting,"missing wait holds common");
        Check(d.Sample(3.5,3).section==Section::Outro,"only valid outro played");
        Check(d.Sample(4,3).finished,"valid outro completes");
        auto inactiveOutro=scene; auto parent=Object("disabled parent"); parent["isActive"]=false; inactiveOutro["sceneObjects"].push_back(parent);
        SectionRoot(inactiveOutro,"disabled outro",2,0,20); inactiveOutro["sceneObjects"].back()["components"][0]["data"]["customData"]["parent"]="disabled parent";
        Check(Compile(inactiveOutro,material).timing[2].seconds==1,"inactive ancestor does not create a duplicate section");
        scene["sceneObjects"][2]["components"][1]["data"]["customData"]["seconds"]=nullptr;
        d=Compile(scene,material); Check(d.timing[0].seconds==0,"null seconds skipped");
        auto invalid=Scene(); SectionRoot(invalid,"intro",0,1,0); Animate(invalid["sceneObjects"][0],"",2,true);
        d=Compile(invalid); Check(d.errors.empty() && d.timing[0].seconds==2,"automatic intro ends after longest clip cycle");
        scene=Scene(); SectionRoot(scene,"common",3,0,0); auto c=Object("target","common");
        std::filesystem::create_directories("Generated/LoadingAnimation");
        unsigned char bmp[58]{}; bmp[0]='B'; bmp[1]='M'; bmp[2]=58; bmp[10]=54; bmp[14]=40; bmp[18]=1; bmp[22]=1; bmp[26]=1; bmp[28]=24; bmp[34]=4;
        bmp[54]=bmp[55]=bmp[56]=255;
        const auto sourceImage=std::filesystem::absolute("Generated/LoadingAnimation/TestSource.bmp");
        { std::ofstream image(sourceImage,std::ios::binary); image.write(reinterpret_cast<const char*>(bmp),sizeof(bmp)); }
        auto render=Component("SpriteRenderer",JSON::object()); render["startupMaterial"]={{"image",sourceImage.generic_string()}};
        c["components"].push_back(render); c["components"][0]["data"]["customData"]["translate"]={{"x",0},{"y",540}};
        c["components"][0]["data"]["customData"]["scale"]={{"x",128},{"y",128}};
        scene["sceneObjects"].push_back(c);
        SectionRoot(scene,"intro",0,1,0); Animate(scene["sceneObjects"][2],"target",4,false,2,1);
        SectionRoot(scene,"waiting",1,1,0); Animate(scene["sceneObjects"][3],"target",1,true);
        SectionRoot(scene,"outro",2,0,2);
        d=Compile(scene); Check(d.errors.empty(),"cross-object common animation valid");
        Check(std::abs(d.timing[0].seconds-1.5)<.0001,"auto duration honors speed and offset");
        Check(std::abs(d.timing[1].seconds-1)<.0001,"loop waiting uses one cycle");
        Check(d.Sample(1.5,0).section==Section::Outro,"early ready skips wait after intro");
        Check(d.Sample(2.2,2).section==Section::Waiting,"cycle-end completion waits");
        Check(d.Sample(2.5,2).section==Section::Outro,"cycle boundary enters outro");
        Check(d.Evaluate(d.Sample(.5,-1))[1].x>0,"another object animated");
        Animate(scene["sceneObjects"][0],"target",10,true);
        d=Compile(scene); Check(std::abs(d.Evaluate(d.Sample(.5,-1))[1].x-50)<.001,"active section wins over common track");
        Frame commonFrame; commonFrame.section=Section::Common; commonFrame.elapsed=1;
        Check(std::abs(d.Evaluate(commonFrame)[1].x-10)<.001,"common-only preview evaluates global time safely");
        scene["sceneObjects"][3]["components"][1]["data"]["customData"]["completion"]=0;
        d=Compile(scene); Check(d.Sample(2.01,2).section==Section::Outro,"immediate completion honored");
        scene["sceneObjects"][2]["components"][2]["data"]["customData"]["animations"][0]["bindings"][0]["targetObjectID"]="deleted";
        d=Compile(scene); Check(d.timing[0].seconds==0,"no effective track skips automatic section");
        scene["sceneObjects"][2]["components"][1]["data"]["customData"]["endMode"]=0;
        scene["sceneObjects"][2]["components"][1]["data"]["customData"]["seconds"]=std::numeric_limits<float>::quiet_NaN();
        d=Compile(scene); Check(d.timing[0].seconds==0,"non-finite manual seconds skipped");
        auto outside=Object("outside"); outside["components"].push_back(Component("SpriteRenderer",{{"pipelineName","custom"}})); scene["sceneObjects"].push_back(outside);
        d=Compile(scene); Check(d.errors.empty(),"invalid skipped section and unrelated objects do not block export");
        const auto folder=std::filesystem::path("Generated/LoadingAnimation/TestPackage"); std::string error;
        Check(Export(d,folder,error),"package export"); const auto loaded=Load(folder/"animation.json");
        Check(loaded.errors.empty(),"package load"); Check(loaded.timing[0].seconds==0 && loaded.timing[2].seconds==2,"roundtrip timing");
        auto cycle=Scene(); auto a=Object("a","b"),b=Object("b","a"); a["components"].push_back(Component("LoadingAnimationSection",{{"section",3}})); cycle["sceneObjects"]={a,b};
        Check(!Compile(cycle).errors.empty(),"parent cycle rejected");
        // Exercise the actual D2D renderer with a native window, including resize and all stages.
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        {
            WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"LoadingAnimationRegression"; RegisterClassW(&wc);
            HWND window=CreateWindowW(wc.lpszClassName,L"Loading test",WS_OVERLAPPEDWINDOW,0,0,640,360,nullptr,nullptr,wc.hInstance,nullptr);
            Check(window!=nullptr,"native preview window"); Renderer renderer;
            Check(renderer.Draw(window,loaded,loaded.Sample(.5,-1)),"D2D common/wait draw");
            SetWindowPos(window,nullptr,0,0,1280,720,SWP_NOMOVE|SWP_NOZORDER);
            Check(renderer.Draw(window,loaded,loaded.Sample(3,2)),"D2D resize/outro draw");
            DestroyWindow(window);
        }
        if(SUCCEEDED(com))CoUninitialize();
        std::cout<<checks<<" loading animation checks passed\n"; return 0;
    }catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
