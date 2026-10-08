// Headless integration tests using the editor build and an isolated temporary EditorTools directory.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Scene/Components/Script/EditorToolManager.h"
#include "Objects/Components/ScriptComponent.h"

using namespace KashipanEngine;
namespace fs = std::filesystem;
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void Write(const fs::path &path, const std::string &text) { std::ofstream out(path); out << text; Require(out.good(), "test file write"); }
void Message(const asSMessageInfo *msg, void *) { std::fprintf(stderr, "%s(%d): %s\n", msg->section, msg->row, msg->message); }
asIScriptEngine *Engine(bool shifted = false) {
    auto *engine = asCreateScriptEngine();
    engine->SetMessageCallback(asFUNCTION(Message), nullptr, asCALL_CDECL);
    if (shifted) engine->RegisterEnum("TypeIdPadding");
    RegisterScriptArray(engine, true);
    RegisterStdString(engine);
    RegisterScriptDictionary(engine);
    RegisterExceptionRoutines(engine);
    RegisterEngineScriptBindings(engine);
    return engine;
}
int Execute(asIScriptEngine *engine, const char *module, const char *function, SceneContext *scene, bool *value = nullptr) {
    auto *context = engine->CreateContext();
    Require(context->Prepare(engine->GetModule(module)->GetFunctionByDecl(function)) >= 0, "prepare regression function");
    ScriptExecutionScope scope(nullptr, scene);
    const int result = context->Execute();
    if (value && result == asEXECUTION_FINISHED) *value = context->GetReturnByte() != 0;
    if (result == asEXECUTION_EXCEPTION) std::fprintf(stdout, "Expected/check exception: %s\n", context->GetExceptionString());
    context->Release();
    return result;
}
void Build(asIScriptEngine *engine, const char *name, const char *source) {
    auto *module = engine->GetModule(name, asGM_ALWAYS_CREATE);
    module->AddScriptSection(name, source);
    Require(module->Build() >= 0, "regression script compilation");
}
int Count(SceneContext *scene, const char *key) {
    auto *value = scene->GetSceneVariable(key);
    return value ? value->AnyCast<int>() : 0;
}
int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto original = fs::current_path();
    const auto sandbox = fs::temp_directory_path() / ("KashipanEditorToolRegression-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(sandbox / "EditorTools");
    fs::current_path(sandbox);
    int exitCode = 0;
    try {
        auto scene = std::make_unique<Scene>(std::string("EditorToolRegression"));
        auto *native = scene->GetSceneContext();
        auto *first = Engine();
        Build(first, "Lifetime", R"(
            Scene@ saved;
            void Capture() { @saved = GetScene(); }
            void Use() { saved.GetName(); }
        )");
        Require(Execute(first, "Lifetime", "void Capture()", native) == asEXECUTION_FINISHED, "capture scene reference");
        auto secondScene = std::make_unique<Scene>(std::string("Second"));
        Require(Execute(first, "Lifetime", "void Use()", secondScene->GetSceneContext()) == asEXECUTION_EXCEPTION, "reject reference in another scene");
        secondScene.reset();
        scene.reset();
        Require(Execute(first, "Lifetime", "void Use()", nullptr) == asEXECUTION_EXCEPTION, "expired scene reference");
        first->ShutDownAndRelease();
        scene = std::make_unique<Scene>(std::string("EditorToolRegression"));
        native = scene->GetSceneContext();

        // Deliberately shift native type ids in another engine.
        auto *editorEngine = Engine(true);
        Write("game.as", R"(
            class Game : ScriptComponentBehavior {
                [SerializeField] string label = "original";
                [SerializeField] Vector3 position;
                [SerializeField] array<float>@ values = null;
                string Echo(const string &in value) { return value; }
            }
        )");
        auto *subject = native->CreateEmptyObject("subject");
        auto *component = subject->AddComponent<ScriptComponent>();
        component->SetScriptPath("game.as");
        Require(component->Reload(), "game component reload");
        Build(editorEngine, "Exchange", R"(
            bool Run() {
                ScriptComponent@ game;
                if (!GetScene().GetObject("subject").GetComponent(@game)) return false;
                string value = "changed";
                if (!game.SetVariable("label", value)) return false;
                string read;
                if (!game.GetVariable("label", read) || read != value) return false;
                Vector3 v(1, 2, 3), copied;
                if (!game.SetVariable("position", v) || !game.GetVariable("position", copied) || copied.y != 2) return false;
                array<float>@ unsupported = { 1, 2 };
                if (game.SetVariable("values", @unsupported) || game.GetVariable("values", @unsupported)) return false;
                if (!game.CallMethod("Echo", value)) return false;
                string returned;
                return game.GetLastReturnValue(returned) && returned == value;
            }
        )");
        bool success = false;
        Require(Execute(editorEngine, "Exchange", "bool Run()", native, &success) == asEXECUTION_FINISHED && success, "cross-engine native exchange and unsafe handle rejection");
        editorEngine->ShutDownAndRelease();
        std::puts("Scene lifetime and cross-engine exchange passed.");

        Write("EditorTools/Shared.as", R"(
            namespace Included {
                [EditorWindow("One"), EditorWindow("Two")]
                class Tool : EditorTool {
                    void InitializeOnLoad() {
                        int count = 0; if (!GetScene().GetVariable("loads", count)) count = 0;
                        int nextCount = count + 1; GetScene().SetVariable("loads", nextCount);
                    }
                    void Update() {
                        int count = 0; if (!GetScene().GetVariable("updates", count)) count = 0;
                        int nextCount = count + 1; GetScene().SetVariable("updates", nextCount);
                        GetScene().SetVariable(GetCurrentEditorWindowName(), true);
                    }
                }
            }
        )");
        Write("EditorTools/Including.as", "#include \"Shared.as\"\n");
        auto &manager = EditorToolManager::GetInstance();
        manager.BeginFrame(native);
        Require(manager.GetLastError().empty(), manager.GetLastError().c_str());
        std::printf("Included tool initialization count: %d\n", Count(native, "loads"));
        Require(Count(native, "loads") == 1, "included tool initialized once");
        manager.UpdateTools();
        Require(Count(native, "updates") == 0, "closed windows do not update");

        Write("EditorTools/Unsafe.as", "class Unsafe : EditorTool { Unsafe() { ImGui::Text(\"outside UI frame\"); } }");
        manager.RequestReload(); manager.BeginFrame(native);
        Require(!manager.GetLastError().empty() && Count(native, "loads") == 1, "constructor UI misuse rejects reload safely");
        fs::remove("EditorTools/Unsafe.as");

        ImGui::CreateContext();
        ImGui::GetIO().DisplaySize = ImVec2(1024, 768);
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        unsigned char *pixels; int width, height;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        manager.OpenWindow("One"); manager.OpenWindow("Two");
        ImGui::NewFrame(); manager.UpdateTools(); ImGui::EndFrame();
        Require(Count(native, "updates") == 2, "both declared windows update");
        Require(native->GetSceneVariable("One") && native->GetSceneVariable("Two"), "current window identity");

        Write("EditorTools/Bad.as", "invalid script source");
        manager.RequestReload(); manager.BeginFrame(native);
        Require(!manager.GetLastError().empty() && manager.IsWindowOpen("One"), "failed reload retains old windows");
        ImGui::NewFrame(); manager.UpdateTools(); ImGui::EndFrame();
        Require(Count(native, "updates") == 4 && Count(native, "loads") == 1, "failed reload retains live instance without initialization");
        fs::remove("EditorTools/Bad.as");
        manager.RequestReload(); manager.BeginFrame(native);
        Require(manager.GetLastError().empty() && !manager.IsWindowOpen("One") && Count(native, "loads") == 2, "successful reload resets instances/windows");

        Write("EditorTools/Throwing.as", R"(
            [EditorWindow("Broken")]
            class Broken : EditorTool {
                void Update() { ImGui::BeginChild("child"); ImGui::BeginDisabled(); throw("expected regression exception"); }
            }
        )");
        manager.RequestReload(); manager.BeginFrame(native); manager.OpenWindow("Broken");
        ImGui::NewFrame();
        const int depth = ImGui::GetCurrentContext()->CurrentWindowStack.Size;
        manager.UpdateTools();
        Require(ImGui::GetCurrentContext()->CurrentWindowStack.Size == depth && ImGui::GetCurrentContext()->DisabledStackSize == 0, "exception restores ImGui child/disabled state");
        Require(!manager.GetLastError().empty(), "tool exception reported");
        ImGui::EndFrame();
        const auto error = manager.GetLastError();
        ImGui::NewFrame(); manager.UpdateTools(); ImGui::EndFrame();
        Require(manager.GetLastError() == error, "failed tool remains stopped");

        Write("EditorTools/Underflow.as", "[EditorWindow(\"Underflow\")] class Underflow : EditorTool { void Update() { ImGui::EndDisabled(); } }");
        manager.RequestReload(); manager.BeginFrame(native); manager.OpenWindow("Underflow");
        ImGui::NewFrame(); ImGui::BeginDisabled(); manager.UpdateTools();
        Require(ImGui::GetCurrentContext()->DisabledStackSize == 1, "script cannot pop caller UI scope");
        Require(!manager.GetLastError().empty(), "UI underflow becomes a script exception");
        ImGui::EndDisabled(); ImGui::EndFrame();
        ImGui::DestroyContext();
        std::puts("Reload, include deduplication, multiple/closed windows and exception recovery passed.");

        // Compile and instantiate the shipped tools in the same loader.
        if (argc > 1) {
            fs::remove("EditorTools/Shared.as"); fs::remove("EditorTools/Including.as"); fs::remove("EditorTools/Throwing.as"); fs::remove("EditorTools/Underflow.as");
            for (const auto &entry : fs::directory_iterator(fs::path(argv[1]))) {
                if (entry.path().extension() == ".as") fs::copy_file(entry.path(), sandbox / "EditorTools" / entry.path().filename());
            }
            manager.RequestReload(); manager.BeginFrame(native);
            Require(manager.GetLastError().empty(), manager.GetLastError().c_str());
            std::puts("Shipped editor tools compile and initialize successfully.");
        }
        scene.reset();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what()); exitCode = 1;
    }
    fs::current_path(original);
    // sandbox is an explicitly created unique child of the system temp directory.
    if (sandbox.parent_path() == fs::temp_directory_path()) fs::remove_all(sandbox);
    return exitCode;
}
