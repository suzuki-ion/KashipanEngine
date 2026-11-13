#if defined(USE_IMGUI)
#include "LoadingAnimationEditor.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Splash/LoadingAnimation.h"
#include "Core/ProjectPaths.h"
#include "Assets/MaterialManager.h"
#include "Assets/TextureManager.h"
#include "Utilities/Conversion/ConvertString.h"
#include <imgui.h>
#include <chrono>
#include <windows.h>
namespace KashipanEngine::LoadingAnimationEditor {
namespace {
bool open=false,playing=false;
HWND preview=nullptr;
LoadingAnimation::Document document;
std::unique_ptr<LoadingAnimation::Renderer> renderer;
float time=0,readyAt=5;
int previewSection=0;
std::chrono::steady_clock::time_point previous;
std::string message;
bool CreateTemplate(SceneEditorCommands *commands) {
    if(!commands)return false;
    std::vector<PasteObjectCommand::Node> nodes;
    const auto spriteID=UUID128(true).ToString();
    std::string commonID;
    auto component=[](const char *type,JSON data) { return JSON{{"type",type},{"data",{{"customData",data}}}}; };
    auto transform=[](float x,float y,float sx,float sy,const std::string &parent) {
        JSON t={{"translate",{{"x",x},{"y",y},{"z",0}}},{"rotate",{{"x",0},{"y",0},{"z",0},{"w",1}}},{"scale",{{"x",sx},{"y",sy},{"z",1}}}};
        if(!parent.empty())t["parent"]=parent; return t;
    };
    for(int section=0;section<4;++section) {
        const auto id=UUID128(true).ToString(); if(section==3)commonID=id;
        JSON object={{"objectID",id},{"name",section==0?"Loading Intro":section==1?"Loading Waiting":section==2?"Loading Outro":"Loading Common"},
            {"components",JSON::array({component("Transform",transform(0,0,1,1,"")),component("LoadingAnimationSection",{{"section",section},{"endMode",1},{"completion",1}})})}};
        if(section<3) {
            const bool waiting=section==1;
            JSON entry={{"name",waiting?"Rotate":"Fade"},{"playOnStart",true},{"loop",waiting},
                {"embeddedAnimation",{{"keyframes",JSON::array({{{"time",0},{"value",section==2?1:0},{"ease",waiting?"EaseOutBack":"Linear"}},{{"time",waiting?1.f:.5f},{"value",waiting?6.2831853f:section==2?0.f:1.f}}})}}},
                {"bindings",JSON::array({{{"targetObjectID",spriteID},{"componentType",waiting?"Transform":"SpriteRenderer"},{"parameterName",waiting?"rotate_":"instanceColor_"},{"channel",waiting?2:3}}})}};
            object["components"].push_back(component("KeyFrameAnimator",{{"animations",JSON::array({entry})}}));
        }
        nodes.push_back({object,-1});
    }
    nodes.push_back({JSON{{"objectID",spriteID},{"name","Loading Sprite"},{"components",JSON::array({
        component("Transform",transform(960,540,192,192,commonID)),
        component("MeshFilter",{{"meshAssetPath","PrimitiveMesh-Rect2D"}}),
        component("SpriteRenderer",{{"materialName","Default"},{"instanceColor",{{"x",1},{"y",1},{"z",1},{"w",0}}}})})}},3});
    return commands->Execute(std::make_unique<PasteObjectCommand>(std::move(nodes),nullptr,MAXSIZE_T,"ロード画面","テンプレート作成"));
}
LRESULT CALLBACK PreviewProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_CLOSE) { ShowWindow(hwnd,SW_HIDE); return 0; }
    if(msg==WM_ERASEBKGND)return 1;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
bool ResolveMaterial(const std::string &name,JSON &out) {
    auto *m=MaterialManager::GetMaterial(name); if(!m)return false;
    auto texture=TextureManager::GetTextureAssetPath(m->textureHandle);
    if(texture.empty() && !m->textureFileName.empty())return false;
    if(!texture.empty()) texture=ProjectPaths::ToPhysical(texture);
    out=JSON{{"image",texture},{"r",m->color.x},{"g",m->color.y},{"b",m->color.z},{"a",m->color.w},
        {"u",m->uvTranslate.x},{"v",m->uvTranslate.y},{"us",m->uvScale.x},{"vs",m->uvScale.y},{"uvRotation",m->uvRotation}};
    return true;
}
void Rebuild(SceneEditorContext *context) {
    renderer.reset(); auto snapshot=context->SaveSceneToJSON();
    for(auto &o:snapshot["sceneObjects"])for(auto &c:o["components"])if(c.value("type",std::string{})=="KeyFrameAnimator") {
        auto &data=c["data"]; auto &custom=data.contains("customData") ? data["customData"] : data;
        for(auto &a:custom["animations"])if(a.contains("jsonPath") && a["jsonPath"].is_string())a["jsonPath"]=ProjectPaths::ToPhysical(a["jsonPath"].get<std::string>());
    }
    document=LoadingAnimation::Compile(snapshot,ResolveMaterial);
    time=0; playing=false; message.clear();
}
void OpenPreview() {
    if(!preview) {
        WNDCLASSW wc{}; wc.lpfnWndProc=PreviewProc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"KashipanLoadingPreview"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); RegisterClassW(&wc);
        preview=CreateWindowW(wc.lpszClassName,L"ロード画面プレビュー (1920 x 1080)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,976,579,nullptr,nullptr,wc.hInstance,nullptr);
    }
    if(preview) { ShowWindow(preview,SW_SHOW); if(!renderer)renderer=std::make_unique<LoadingAnimation::Renderer>(); }
}
}
void Menu() { ImGui::MenuItem("ロード画面の作成",nullptr,&open); }
void Close() { renderer.reset(); if(preview)DestroyWindow(preview); preview=nullptr; playing=false; }
void Show(SceneEditorContext *context, SceneEditorCommands *commands) {
    if(!open) { Close(); return; }
    const auto now=std::chrono::steady_clock::now();
    if(playing)time+=std::min(.1f,std::chrono::duration<float>(now-previous).count()); previous=now;
    if(ImGui::Begin("ロード画面の作成",&open)) {
        ImGui::TextWrapped("通常のシーンに区間ごとの親オブジェクトを作り、LoadingAnimationSectionを追加します。子にSpriteRendererを配置し、KeyFrameAnimatorの自動再生を有効にしてください。共通区間の子は全区間で表示します。");
        ImGui::TextWrapped("1920x1080 / 左下原点 / +Y上 / 回転ラジアン。実際のウィンドウサイズへ縦横比を保って拡縮します。");
        if(ImGui::Button("4区間のテンプレートをシーンに追加 (Undo対応)")) {
            message=CreateTemplate(commands) ? "テンプレートを追加しました。Loading Spriteのマテリアルを変更してください。" : "追加できませんでした。";
        }
        if(ImGui::Button("現在のシーンを検証・プレビューへ反映"))Rebuild(context);
        for(int s=0;s<3;++s) { const auto &t=document.timing[s]; ImGui::Text("%s: %.3f 秒 (%s)",s==0?"開始":s==1?"待機周期":"終了",t.seconds,t.source.c_str()); }
        for(const auto &e:document.errors)ImGui::TextWrapped("エラー: %s",e.c_str());
        for(const auto &w:document.warnings)ImGui::TextWrapped("注意: %s",w.c_str());
        ImGui::Combo("プレビュー区間",&previewSection,"全体\0開始\0完了待ち\0終了\0共通のみ\0");
        ImGui::InputFloat("読み込み完了の想定秒数 (-1 = 未完了)",&readyAt);
        if(ImGui::Button(playing?"一時停止":"再生")) { if(document.package.empty())Rebuild(context); OpenPreview(); playing=!playing; previous=now; }
        ImGui::SameLine(); if(ImGui::Button("先頭へ"))time=0;
        if(ImGui::InputFloat("再生位置 (秒)",&time)) { time=std::max(0.f,time); OpenPreview(); }
        ImGui::TextWrapped("書き出すと、このプロジェクトのRelease起動アニメーションとして使用します。Developmentの起動表示は従来どおりです。");
        if(ImGui::Button("現在のシーンを書き出して起動時に使用")) {
            Rebuild(context); std::string error;
            if(LoadingAnimation::Export(document,Utf8StringToPath(ProjectPaths::ProjectRoot())/"StartupAnimation",error))message="書き出しました。Releaseの次回起動から反映します。"; else message=error;
        }
        ImGui::TextWrapped("%s",message.c_str());
        ImGui::TextWrapped("エンジン既定へ戻す場合はプロジェクトのStartupAnimation/animation.jsonを移動してください。シーン編集内容は通常のシーン保存で保存します。");
    }
    ImGui::End();
    if(preview && IsWindowVisible(preview) && renderer && document.errors.empty()) {
        auto frame=document.Sample(time,readyAt);
        if(previewSection>0) {
            frame.section=static_cast<LoadingAnimation::Section>(previewSection-1); frame.elapsed=time; frame.local=time; frame.finished=false;
            if(previewSection<=3 && document.timing[previewSection-1].seconds>0)frame.local=std::fmod(time,document.timing[previewSection-1].seconds);
        }
        renderer->Draw(preview,document,frame);
    }
}
}
#endif
