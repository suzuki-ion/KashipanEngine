#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Transform.h"
using namespace KashipanEngine;
static void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main() {
    const auto original = std::filesystem::current_path();
    const auto sandbox = std::filesystem::temp_directory_path() / ("KashipanRendering-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox); std::filesystem::current_path(sandbox);
    int result = 0;
    try {
        Scene scene(std::string("RenderingRegression"));
        auto *object = scene.GetSceneContext()->CreateEmptyObject("sprite");
        object->SetActive(false);
        auto *sprite = object->AddComponent<SpriteRenderer>();
        Require(sprite != nullptr,"sprite creation");
        ShortText::CommandContext context{object, object, scene.GetSceneContext(), nullptr, 0};
        std::string error;
        auto execute = [&](const char *name, const ShortText::Value &args) { return ShortText::ExecuteRenderingCommand(name,args,context,error); };
        Require(execute("render.uv",{{"offset",{0.25,0.5}},{"size",{0.25,0.5}},{"flipX",true}}),"uv command");
        Require(sprite->GetInstanceUvTranslate().x == 0.5f && sprite->GetInstanceUvScale().x == -0.25f,"crop flip offsets");
        const auto before = sprite->GetInstanceUvScale();
        Require(!execute("render.uv",{{"size",{0,-1}}}) && !error.empty(),"invalid uv rejected");
        Require(sprite->GetInstanceUvScale() == before,"invalid UV retains state");
        Require(execute("render.layer",{{"layer","Overlay"}}) && sprite->GetRenderPriority() == 20000,"named layer map");
        Require(!execute("render.order",{{"priority",0.5}}),"fractional render priority rejected");
        Require(sprite->GetRenderPriority() == 20000,"invalid order retains state");
        Require(!execute("component.remove",{{"type","Transform"}}),"mandatory transform protected");
        Require(execute("component.add",{{"type","Camera2D"}}),"registered component add");
        Require(!execute("component.add",{{"type","Camera2D"}}),"duplicate component rejected");
        Require(execute("camera.configure",{{"mode","2d"},{"values",{{"width_",640.0},{"height_",360.0}}}}),"camera properties configured");
        Require(object->GetComponent<Camera2D>()->GetWidth() == 640,"camera setter effect");
        Require(!execute("component.configure",{{"type","Camera2D"},{"values",{{"width_",100.0},{"unknown_",true}}}}),"unknown member rejected");
        Require(object->GetComponent<Camera2D>()->GetWidth() == 640,"configuration validated before mutation");
        Require(execute("component.remove",{{"type","Camera2D"}}),"optional component removal");
        Require(object->GetComponent<Camera2D>() == nullptr,"component removed");
        auto *bufferObject = scene.GetSceneContext()->CreateEmptyObject("Buffer"); bufferObject->SetActive(false);
        auto *buffer = bufferObject->AddComponent<ScreenBufferObject>(); Require(buffer != nullptr,"inactive buffer descriptor creation");
        buffer->SetSize(640,360);
        auto *cameraObject = scene.GetSceneContext()->CreateEmptyObject("Camera"); cameraObject->SetActive(false);
        auto *camera = cameraObject->AddComponent<Camera2D>(); Require(camera != nullptr,"inactive camera creation"); camera->SetAutoSyncSize(true);
        auto *cameraRenderer = cameraObject->AddComponent<CameraRenderer>(); Require(cameraRenderer != nullptr,"inactive camera renderer"); cameraRenderer->SetTargetObject(bufferObject);
        Require(execute("render.layout",{{"sourceName","Buffer"},{"cover",true}}),"layout without GPU target");
        Require(object->GetComponent<Transform>()->GetScale().x == 640 && object->GetComponent<Transform>()->GetScale().y == 360,"cover uses configured buffer size");
        buffer->SetSize(800,600);
        Require(execute("render.layout",{{"sourceName","Buffer"},{"cover",true},{"edge","top_right"}}),"layout updates resize");
        Require(object->GetComponent<Transform>()->GetTranslate().x == 800 && object->GetComponent<Transform>()->GetTranslate().y == 600,"screen edge coordinates follow orthographic projection");
        Require(sprite->GetTargetObjectID() == bufferObject->GetObjectID(),"layout routes renderer");
        auto *text = object->AddComponent<TextRenderer>(); Require(text != nullptr,"text creation");
        Require(execute("text.style",{{"size",28.0},{"color",{0.25,0.5,1,1}},{"alpha",0.5}}),"text styling");
        Require(text->GetFontSize() == 28 && text->GetColor().w == 0.5f,"text style setters");
        Require(!execute("text.style",{{"size",48},{"alpha",2}}),"invalid alpha rejected");
        Require(text->GetFontSize() == 28,"text style atomic validation");
        Require(execute("text.format",{{"value",42},{"width",5},{"zeroPad",true},{"prefix","Score: "}}),"formatted number");
        Require(text->GetText() == "Score: 00042","text numeric format");
        Require(!execute("not.a.render.command",ShortText::Value::object()) && error.empty(),"unknown command passes to other adapters");
        std::puts("ShortText rendering adapter regression passed (headless properties only; no GPU/window/font rendering).");
    } catch (const std::exception &exception) { std::fprintf(stderr,"%s\n",exception.what()); result = 1; }
    std::filesystem::current_path(original); std::filesystem::remove_all(sandbox); return result;
}
