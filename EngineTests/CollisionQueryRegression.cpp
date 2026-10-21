// Headless integration tests: production physics, scene snapshots and AngelScript bindings.
#include <stdexcept>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include "Objects/Collision/CollisionQuery.h"
#include "Objects/Components/Collider/Box2DCollider.h"
#include "Objects/Components/Collider/SphereCollider.h"
#include "Objects/Components/Collider/BoxCollider.h"
#include "Objects/Components/Transform.h"
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/Components/Script/ScriptBindings.h"

using namespace KashipanEngine;

void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void Message(const asSMessageInfo *message, void *) {
    std::fprintf(stderr, "%s(%d): %s\n", message->section, message->row, message->message);
}

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        // Every 2D shape pair, including segment-box/capsule and rotated boxes.
        for (int a = 0; a < 5; ++a) for (int b = 0; b < 5; ++b) {
            CollisionShape2D x, y;
            x.type = static_cast<CollisionShapeType2D>(a);
            y.type = static_cast<CollisionShapeType2D>(b);
            x.start = y.start = Vector2{-1, 0};
            x.end = y.end = Vector2{1, 0};
            x.rotation = 0.5f;
            Require(ComputeQueryHit2D(x.Build(), y.Build()).isHit, "2D shape pair missed");
        }
        Collider physics;
        std::puts("2D shape pairs passed");
        CollisionShape3D sphere;
        ColliderInfo3D target;
        target.shape = ColliderInfo3D::SphereShape3D{Vector3{0.75f, 0, 0}, 0.5f};
        auto hits = physics.Query3D(sphere.Build(), {}, sphere.rotation, {target});
        Require(hits.size() == 1 && hits[0].normal.x < -0.9f && hits[0].penetration > 0.2f, "3D hit details");
        Require(physics.Query3D(sphere.Build(), {10, 0, 0}, sphere.rotation, {target}).empty(), "3D separation");
        CollisionShape3D box;
        box.type = CollisionShapeType3D::Box;
        box.halfExtents = {2, 0.1f, 0.1f};
        target.shape = ColliderInfo3D::SphereShape3D{Vector3{0, 1.5f, 0}, 0.2f};
        Require(physics.Query3D(box.Build(), {}, box.rotation, {target}).empty(), "unrotated box");
        Require(physics.Query3D(box.Build(), {}, Quaternion::MakeRotateEuler({0, 0, 1.5707963f}), {target}).size() == 1, "rotated box");
        CollisionShape3D capsule;
        capsule.type = CollisionShapeType3D::Capsule;
        Require(physics.Query3D(capsule.Build(), {}, capsule.rotation, {sphere.Build()}).size() == 1, "capsule query");
        CollisionShape3D mesh;
        mesh.type = CollisionShapeType3D::ConvexMesh;
        mesh.vertices = {{-1,-1,-1}, {1,-1,-1}, {0,1,-1}, {0,0,1}};
        Require(physics.Query3D(mesh.Build(), {}, mesh.rotation, {sphere.Build()}).size() == 1, "convex query");
        ColliderInfo3D triangles;
        std::puts("3D primitive and convex queries passed");
        triangles.shape = ColliderInfo3D::ConcaveMeshShape3D{
            {{-2,0,-2}, {2,0,-2}, {2,0,2}, {-2,0,2}}, {0,2,1, 0,3,2}};
        Require(physics.Query3D(sphere.Build(), {0,0.1f,0}, sphere.rotation, {triangles}).size() == 1, "mesh target");
        sphere.radius = -1;
        Require(physics.Query3D(sphere.Build(), {}, sphere.rotation, {target}).empty(), "invalid dimensions");

        Scene scene(std::string("CollisionQueryRegression"));
        std::puts("3D mesh and invalid dimensions passed");
        auto *ctx = scene.GetSceneContext();
        auto *queries = ctx->AddComponent<SceneObjectCollider>();
        auto *object = ctx->CreateEmptyObject("target");
        auto *transform = object->GetComponent<Transform>();
        auto *collider = object->AddComponent<SphereCollider>();
        auto *collider2D = object->AddComponent<Box2DCollider>();
        Require(queries && transform && collider && collider2D, "scene setup");
        int events = 0;
        collider->SetOnCollisionEnter3D([&](const auto &) { ++events; });
        CollisionShape3D shape;
        Require(queries->Query3D(shape.Build(), {}, shape.rotation).size() == 1, "scene initial snapshot");
        transform->SetTranslate({10, 0, 0});
        Require(queries->Query3D(shape.Build(), {}, shape.rotation).empty(), "stale transform in query");
        transform->SetTranslate({0, 0, 0});
        collider->SetTrigger(true);
        Require(queries->Query3D(shape.Build(), {}, shape.rotation, false).empty(), "trigger filter");
        Require(queries->Query3D(shape.Build(), {}, shape.rotation, true, object).empty(), "object filter");
        ColliderInfo2D shape2D;
        shape2D.shape = Math::Circle{{0,0}, 0.5f};
        Require(queries->Query2D(shape2D).size() == 1, "scene 2D snapshot");
        collider2D->SetActive(false);
        Require(queries->Query2D(shape2D).empty(), "inactive filter");
        collider2D->SetActive(true);
        Require(events == 0 && queries->GetCollider()->GetPhysicsWorld()->getNbRigidBodies() == 0, "query modified live physics");

        auto *engine = asCreateScriptEngine();
        std::puts("Scene snapshot and filtering cases passed");
        engine->SetMessageCallback(asFUNCTION(Message), nullptr, asCALL_CDECL);
        RegisterScriptArray(engine, true);
        RegisterStdString(engine);
        RegisterScriptDictionary(engine);
        RegisterExceptionRoutines(engine);
        RegisterEngineScriptBindings(engine);
        std::puts("AngelScript registration completed");
        auto *module = engine->GetModule("QueryTest", asGM_ALWAYS_CREATE);
        const char *script = R"(
            CollisionHit@ saved;
            bool Run() {
                CollisionShape3D s;
                array<CollisionHit@>@ hits = QueryCollision3D(s);
                if (hits.length() != 1 || hits[0].otherObject.GetName() != "target") return false;
                @saved = hits[0];
                @hits = null;
                if (saved.otherCollider is null) return false;
                if (QueryCollision3D(s, false).length() != 0) return false;
                if (QueryCollision3D(s, true, saved.otherObject).length() != 0) return false;
                if (QueryCollider(saved.otherCollider).length() != 0) return false;
                s.type = CollisionShapeType3D::ConvexMesh;
                s.SetVertices({Vector3(-1,-1,-1), Vector3(1,-1,-1), Vector3(0,1,-1), Vector3(0,0,1)});
                CollisionShape3D copy = s;
                if (QueryCollision3D(copy).length() != 1) return false;
                CollisionShape2D d;
                d.type = CollisionShapeType2D::Box;
                d.rotation = 0.5;
                return QueryCollision2D(d).length() == 1;
            }
            void Invalid() { CollisionShape3D s; s.radius = -1; QueryCollision3D(s); }
            void Deleted() { saved.otherObject.GetName(); }
            void NoScene() { CollisionShape2D s; QueryCollision2D(s); }
        )";
        module->AddScriptSection("query.as", script);
        Require(module->Build() >= 0, "AngelScript registration/compilation");
        auto *execution = engine->CreateContext();
        {
            ScriptExecutionScope scope(nullptr, ctx);
            execution->Prepare(module->GetFunctionByDecl("bool Run()"));
            Require(execution->Execute() == asEXECUTION_FINISHED && execution->GetReturnByte() != 0, "AngelScript execution");
            execution->Prepare(module->GetFunctionByDecl("void Invalid()"));
            Require(execution->Execute() == asEXECUTION_EXCEPTION, "invalid shape exception");
            ctx->DeleteObject(object);
            execution->Prepare(module->GetFunctionByDecl("void Deleted()"));
            Require(execution->Execute() == asEXECUTION_EXCEPTION, "deleted target handle safety");
        }
        execution->Prepare(module->GetFunctionByDecl("void NoScene()"));
        Require(execution->Execute() == asEXECUTION_EXCEPTION, "missing scene exception");
        if (argc > 1) Require(GenerateScriptPredefinedFile(engine, argv[1]), "predefined export");
        if (argc > 2) {
            std::ifstream input(argv[2]);
            Require(input.is_open(), "usage example input");
            const std::string example((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            auto *exampleModule = engine->GetModule("Example", asGM_ALWAYS_CREATE);
            exampleModule->AddScriptSection(argv[2], example.c_str());
            Require(exampleModule->Build() >= 0, "usage example compilation");
        }
        execution->Release();
        engine->ShutDownAndRelease();
        std::puts("Collision query physics, scene snapshots and AngelScript regression cases passed.");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAILED: %s\n", e.what());
        return 1;
    }
}
