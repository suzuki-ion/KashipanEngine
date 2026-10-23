// Production AngelScript bindings and WFC, without GPU/window initialization.
#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptbuilder/scriptbuilder.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ScriptComponent.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Transform.h"
using namespace KashipanEngine;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void Message(const asSMessageInfo *m, void *) { std::fprintf(stderr, "%s:%d %s\n", m->section, m->row, m->message); }
int Include(const char *name, const char *from, CScriptBuilder *builder, void *) {
    const auto path = std::filesystem::path(from).parent_path() / name;
    return builder->AddSectionFromFile(path.string().c_str());
}
int main(int argc, char **argv) {
    try {
        Require(argc == 3, "expected repository root and isolated scratch directory");
        auto root = std::filesystem::absolute(argv[1]);
        auto scratch = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(scratch / "Assets/Application/Data");
        std::filesystem::copy_file(root / "GameProjects/JobHuntingGame/Assets/Application/Data/LightTownRules.json",
            scratch / "Assets/Application/Data/LightTownRules.json", std::filesystem::copy_options::overwrite_existing);
        std::filesystem::current_path(scratch);
        Scene scene(std::string("CityScene"));
        // Hidden UI must unregister before its pooled storage can be destroyed/reused.
        // The former Finalize used IsActive() after its owner was already inactive.
        {
            auto *owner = scene.GetSceneContext()->CreateEmptyObject("HiddenUIRegression");
            auto *sprite = owner->AddComponent<SpriteRenderer>();
            auto *renderer = scene.GetSceneContext()->GetComponent<SceneRenderer>();
            Require(renderer != nullptr, "sprite renderer registry exists");
            const auto baseline = renderer->GetSpriteRenderers().size() - 1;
            auto registered = [&] { const auto &list = renderer->GetSpriteRenderers(); return std::find(list.begin(), list.end(), sprite) != list.end(); };
            Require(registered(), "active sprite registered");
            for (int cycle = 0; cycle < 4; ++cycle) {
                owner->SetActive(false); Require(!registered(), "hidden sprite unregisters immediately");
                owner->SetActive(true); Require(registered(), "reactivated sprite registers again");
                Require(renderer->GetSpriteRenderers().size() == baseline + 1, "no duplicate renderer registration");
            }
            owner->SetActive(false); scene.GetSceneContext()->DeleteObject(owner);
            Require(renderer->GetSpriteRenderers().size() == baseline, "hidden sprite deletion leaves no dangling registry entry");
            for (int cycle = 0; cycle < 32; ++cycle) {
                auto *parent = scene.GetSceneContext()->CreateEmptyObject("HiddenUIParent");
                auto *child = scene.GetSceneContext()->CreateEmptyObject("HiddenUIChild");
                child->GetComponent<Transform>()->SetParentObject(parent); child->AddComponent<SpriteRenderer>();
                parent->SetActive(false);
                Require(renderer->GetSpriteRenderers().size() == baseline, "parent deactivation unregisters child sprite");
                scene.GetSceneContext()->DeleteObject(parent);
                Require(renderer->GetSpriteRenderers().size() == baseline, "hidden subtree destruction and slot reuse stay clean");
            }
        }
        // The plain compiler does not enforce ScriptComponent's GC-field restrictions.
        // Exercise the actual production loader, including behavior construction.
        for (const char *name : {"TitleController", "CityController", "ExpeditionController", "../MoveGround", "../AlwaysRotate", "../TeleportFloor", "../Checkpoint", "../Enemy"}) {
            auto *object = scene.GetSceneContext()->CreateEmptyObject(name);
            auto *script = object->AddComponent<ScriptComponent>();
            script->SetScriptPath((root / "GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown" / (std::string(name) + ".as")).string());
            if (!script->Reload()) {
                std::fprintf(stderr, "Loader rejected %s: %s\n", name, script->GetLastError().c_str());
                throw std::runtime_error("production ScriptComponent load");
            }
        }
        auto *engine = asCreateScriptEngine();
        engine->SetMessageCallback(asFUNCTION(Message), nullptr, asCALL_CDECL);
        RegisterScriptArray(engine, true); RegisterStdString(engine);
        RegisterScriptDictionary(engine); RegisterExceptionRoutines(engine);
        RegisterEngineScriptBindings(engine);
        CScriptBuilder builder;
        Require(builder.StartNewModule(engine, "LightTown") >= 0, "module");
        builder.SetIncludeCallback(Include, nullptr);
        auto source = (root / "EngineTests/JobHuntingLightTownRegression.as").string();
        Require(builder.AddSectionFromFile(source.c_str()) >= 0 && builder.BuildModule() >= 0, "script compile");
        auto *context = engine->CreateContext();
        {
            ScriptExecutionScope scope(nullptr, scene.GetSceneContext());
            context->Prepare(builder.GetModule()->GetFunctionByDecl("void Run()"));
            if (context->Execute() != asEXECUTION_FINISHED) {
                std::fprintf(stderr, "Script exception line %d: %s\n", context->GetExceptionLineNumber(), context->GetExceptionString());
                throw std::runtime_error("gameplay regression");
            }
        }
        context->Release(); engine->ShutDownAndRelease();
        std::puts("PASS: production ScriptComponent loading (title/city/expedition), build transactions, shop unlock, dispatch persistence, city-only victory, 128 WFC seeds. GPU/gameplay visuals not tested.");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "FAILED: %s\n", e.what()); return 1; }
}
