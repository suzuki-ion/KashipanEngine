#include "Scenes/Components/AttackBase.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/3D/Material3D.h"
#include "Utilities/RandomValue.h"
#include "Assets/AudioManager.h"
#include "Objects/SystemObjects/PointLight.h"

namespace KashipanEngine {

Plane3D *AttackBase::SpawnXZPlaneWithMoves(const std::string &name, const Vector3 &spawnPos, const std::vector<MoveEntry> &extraMoves) {
    auto *ctx = GetOwnerContext();
    if (!ctx) return nullptr;

    auto plane = std::make_unique<Plane3D>();
    plane->SetName(name);

    // Collision (AABB) - 0.8 倍想定
    if (sceneDefault_) {
        if (auto *colliderComp = sceneDefault_->GetColliderComp()) {
            if (colliderComp->GetCollider()) {
                ColliderInfo3D info;
                Math::AABB aabb;
                // Plane3D は基本 1x1 を scale で拡大する想定。見た目より 0.8 倍。
                // ローカル AABB を [-0.4, -0.4]..[+0.4, +0.4]（XZ）として定義し、Y は薄く。
                aabb.min = Vector3{-0.4f, -0.05f, -0.4f};
                aabb.max = Vector3{+0.4f, +0.05f, +0.4f};
                info.shape = aabb;
                plane->RegisterComponent<Collision3D>(colliderComp->GetCollider(), info);
            }
        }
    }

    if (auto *tr = plane->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{spawnPos.x, 32.0f, spawnPos.z});
        tr->SetScale(Vector3{ 1.5f, 1.5f, 1.0f });

        if (mover_) {
            if (auto *moverTr = mover_->GetComponent3D<Transform3D>()) {
                tr->SetParentTransform(moverTr);
            }
        }
    }
    if (auto *material = plane->GetComponent3D<Material3D>()) {
        material->SetTexture(TextureManager::GetTextureFromFileName("gears.png"));
        Material3D::UVTransform uvTrans;
        uvTrans.scale = Vector3{ 0.25f, 1.0f, 1.0f };
        uvTrans.translate.x = static_cast<float>(GetRandomInt(0, 3)) * 0.25f;
        material->SetUVTransform(uvTrans);
    }

    std::vector<MoveEntry> moves;
    moves.reserve(2 + extraMoves.size());

    moves.push_back(MakeCommonSpawnDropMove(spawnPos));
    moves.insert(moves.end(), extraMoves.begin(), extraMoves.end());
    const Vector3 lastTo = moves.back().to;
    moves.push_back(MakeCommonFinalRiseMove(lastTo));

    plane->RegisterComponent<MovementController>(moves);
    if (auto *mc = plane->GetComponent3D<MovementController>()) {
        mc->Start();
    }
    plane->RegisterComponent<AlwaysRotate>(Vector3{ 0.0f, 0.0f, 8.0f });

    // ScreenBuffer へのアタッチ
    if (sceneDefault_) {
        if (auto *screenBuffer = sceneDefault_->GetScreenBuffer3D()) {
            plane->AttachToRenderer(screenBuffer, "Object3D.Solid.BlendNormal");
        }
        if (auto *shadowMapBuffer = sceneDefault_->GetShadowMapBuffer()) {
            plane->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        }
    }

    Plane3D *outPtr = plane.get();
    ctx->AddObject3D(std::move(plane));
    if (outPtr) spawnedObjects_.push_back(outPtr);

    if (spawnWithPointLight_ && sceneDefault_) {
        const size_t objCount = spawnedObjects_.size();
        if (pointLights_.size() < objCount) {
            size_t toCreate = objCount - pointLights_.size();
            for (size_t i = 0; i < toCreate; ++i) {
                auto pl = std::make_unique<PointLight>();
                pl->SetName(name + "_PointLight_" + std::to_string(pointLights_.size()));
                pl->SetEnabled(true);
                pl->SetColor(Vector4{1.0f, 0.8f, 0.6f, 1.0f});
                pl->SetIntensity(3.0f);
                pl->SetRange(5.0f);

                PointLight *plPtr = pl.get();
                if (sceneDefault_) {
                    if (auto *screenBuffer = sceneDefault_->GetScreenBuffer3D()) {
                        pl->AttachToRenderer(screenBuffer, "Object3D.Solid.BlendNormal");
                    }
                }
                ctx->AddObject3D(std::move(pl));
                if (plPtr) {
                    pointLights_.push_back(plPtr);
                    if (auto *lm = sceneDefault_->GetLightManager()) {
                        lm->AddPointLight(plPtr);
                    }
                }
            }
        }

        for (size_t i = 0; i < pointLights_.size(); ++i) {
            PointLight *pl = pointLights_[i];
            if (!pl) continue;
            if (i < spawnedObjects_.size()) {
                auto *obj = spawnedObjects_[i];
                if (obj) {
                    if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                        Matrix4x4 mat = tr->GetWorldMatrix();
                        Vector3 pos = { mat.m[3][0], mat.m[3][1] + 0.5f, mat.m[3][2] };
                        pl->SetEnabled(true);
                        pl->SetPosition(pos);
                    }
                }
            } else {
                pl->SetEnabled(false);
                pl->SetPosition(Vector3{10000.0f, 10000.0f, 10000.0f});
            }
        }
    }

    auto soundHandle = AudioManager::GetSoundHandleFromFileName("gearIn.mp3");
    AudioManager::Play(soundHandle, 0.5f, 0.0f, false);
    return outPtr;
}

void AttackBase::Update() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    // MovementController が終わったオブジェクトから順に削除
    // ※ 初期の生成順で「終わったものから先に」消す想定。
    for (size_t i = 0; i < spawnedObjects_.size();) {
        auto *obj = spawnedObjects_[i];
        if (!obj) {
            spawnedObjects_.erase(spawnedObjects_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        for (size_t li = 0; li < pointLights_.size(); ++li) {
            PointLight *pl = pointLights_[li];
            if (!pl) continue;
            if (li < spawnedObjects_.size()) {
                auto *o = spawnedObjects_[li];
                if (o) {
                    if (auto *tr = o->GetComponent3D<Transform3D>()) {
                        Matrix4x4 mat = tr->GetWorldMatrix();
                        Vector3 pos = { mat.m[3][0], mat.m[3][1] + 0.5f, mat.m[3][2] };
                        pl->SetEnabled(true);
                        pl->SetPosition(pos);
                    }
                }
            } else {
                pl->SetEnabled(false);
                pl->SetPosition(Vector3{10000.0f, 10000.0f, 10000.0f});
            }
        }

        auto *mc = obj->GetComponent3D<MovementController>();
        if (mc && mc->GetCurrentIndex() == 2 && mc->GetPreviousIndex() == 1) {
            // 最終上昇動作に入った直後に効果音を鳴らす
            auto soundHandle = AudioManager::GetSoundHandleFromFileName("gearOut.mp3");
            AudioManager::Play(soundHandle, 0.5f, 0.0f, false);
        }
        if (mc && !mc->IsMoving()) {
            ctx->RemoveObject3D(obj);
            spawnedObjects_.erase(spawnedObjects_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        ++i;
    }
}

} // namespace KashipanEngine
