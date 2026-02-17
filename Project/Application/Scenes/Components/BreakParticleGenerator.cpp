#include "Scenes/Components/BreakParticleGenerator.h"

#include "Scene/SceneContext.h"
#include "Objects/Components/AlwaysRotate.h"
#include "Utilities/RandomValue.h"
#include "Objects/Components/MovementController.h"

#include <cmath>
#include <algorithm>

namespace KashipanEngine {

void BreakParticleGenerator::Generate(const Vector3 &pos) {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    constexpr int kCount = 10;

    constexpr float kMoveDuration = 0.35f;
    constexpr float kMoveDistanceMin = 1.0f;
    constexpr float kMoveDistanceMax = 3.0f;

    for (int i = 0; i < kCount; ++i) {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName(std::string("BreakParticle_") + std::to_string(serial_++) + "_" + std::to_string(i));

        obj->RegisterComponent<AlwaysRotate>(Vector3(0.0f, 0.0f, 4.0f));

        {
            Vector3 dir{
                GetRandomFloat(-1.0f, 1.0f),
                GetRandomFloat(0.2f, 1.0f),
                GetRandomFloat(-1.0f, 1.0f)};
            const float len = dir.Length();
            if (len > 0.0001f) dir = dir / len;
            const float dist = GetRandomFloat(kMoveDistanceMin, kMoveDistanceMax);

            std::vector<MovementController::MoveEntry> moves;
            MovementController::MoveEntry entry;
            entry.from = pos;
            entry.to = pos + dir * dist;
            entry.duration = kMoveDuration;
            entry.easing = [](Vector3 a, Vector3 b, float t) {
                return EaseOutCubic(a, b, t);
            };
            moves.push_back(std::move(entry));

            obj->RegisterComponent<MovementController>(std::move(moves));
        }

        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{pos.x, pos.y, pos.z});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});

            if (mover_) {
                if (auto *moverTr = mover_->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(moverTr);
                }
            }
        }

        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("gears.png"));
            mat->SetEnableLighting(false);
            mat->SetColor(Vector4{1.0f, 0.0f, 0.0f, 1.0f});
            Material3D::UVTransform uv{};
            uv.scale = Vector3{0.25f, 1.0f, 1.0f};
            uv.translate = Vector3{static_cast<float>(GetRandomInt(0, 3)) * 0.25f, 0.0f, 0.0f};
            mat->SetUVTransform(uv);
        }

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }

        auto *raw = obj.get();
        particles_.push_back(raw);

        if (auto *mc = raw->GetComponent3D<MovementController>()) {
            mc->Start();
        }

        ctx->AddObject3D(std::move(obj));
    }
}

void BreakParticleGenerator::Update() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(), [&](Object3DBase *obj) {
            if (!obj) return true;
            auto *mc = obj->GetComponent3D<MovementController>();
            if (!mc) return false;
            if (mc->IsMoving()) return false;

            ctx->RemoveObject3D(obj);
            return true;
        }),
        particles_.end());
}

} // namespace KashipanEngine
