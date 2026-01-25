#include "Scenes/Components/JustAvoidParticle.h"
#include "Objects/Components/MovementController.h"
<<<<<<< HEAD:Project/Application/Scenes/Components/JustAvoidParticle.cpp
#include "Scene/SceneContext.h"
=======
>>>>>>> TD2_3:Application/Scenes/Components/JustAvoidParticle.cpp
#include <algorithm>
#include <cmath>
#include <random>

namespace KashipanEngine {

namespace {
Vector3 NormalizeSafe(const Vector3& v) {
    const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    if (len2 <= 0.000001f) return Vector3{ 0.0f, 0.0f, 1.0f };
    const float inv = 1.0f / std::sqrt(len2);
    return Vector3{ v.x * inv, v.y * inv, v.z * inv };
}

constexpr Vector3 kHiddenPos{ -10000.0f, -10000.0f, -10000.0f };

Vector3 MakeRandomDirectionNear(const Vector3& dir) {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const Vector3 base = NormalizeSafe(dir);

    // Small random jitter
    Vector3 jitter{ dist(rng), dist(rng), dist(rng) };
    // favor around base direction
    Vector3 out{ base.x + jitter.x * 0.35f, base.y + jitter.y * 0.35f, base.z + jitter.z * 0.35f };
    return NormalizeSafe(out);
}
} // namespace

void JustAvoidParticle::Initialize() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;
    sceneDefault_ = ctx->GetComponent<SceneDefaultVariables>("SceneDefaultVariables");
    camera3D_ = sceneDefault_ ? sceneDefault_->GetMainCamera3D() : nullptr;

    const auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");
    const auto textTex = TextureManager::GetTextureFromFileName("avoidJustText.png");

    ScreenBuffer* sb = sceneDefault_ ? sceneDefault_->GetScreenBuffer3D() : nullptr;

    // particle billboards
    for (size_t i = 0; i < billboards_.size(); ++i) {
        auto obj = std::make_unique<Billboard>();
        obj->SetName(std::string("JustAvoidParticle_") + std::to_string(i));
        obj->SetCamera(camera3D_);
        obj->SetFacingMode(Billboard::FacingMode::MatchCameraRotation);

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 0.2f, 0.2f, 0.2f });
            tr->SetTranslate(kHiddenPos);
            if (mover_) {
                if (auto* moverTr = mover_->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(moverTr);
                }
            }
        }

        if (auto* mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetTexture(whiteTex);
            auto c = mat->GetColor();
            c.w = 0.0f;
            mat->SetColor(c);
        }

        obj->RegisterComponent<MovementController>(std::vector<MovementController::MoveEntry>{});

        if (sb) {
            obj->AttachToRenderer(sb, "Object3D.Solid.BlendNormal");
        }

        billboards_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    // text billboard
    {
        auto obj = std::make_unique<Billboard>();
        obj->SetUniqueBatchKey();
        obj->SetName("JustAvoidText");
        obj->SetCamera(camera3D_);
        obj->SetFacingMode(Billboard::FacingMode::MatchCameraRotation);

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 4.68f, 2.0f, 1.0f });
            tr->SetTranslate(kHiddenPos);
            if (mover_) {
                if (auto* moverTr = mover_->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(moverTr);
                }
            }
        }

        if (auto* mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetTexture((textTex != TextureManager::kInvalidHandle) ? textTex : whiteTex);
            auto c = mat->GetColor();
            c.w = 0.0f;
            mat->SetColor(c);
        }

        obj->RegisterComponent<MovementController>(std::vector<MovementController::MoveEntry>{});

        if (sb) {
            obj->AttachToRenderer(sb, "Object3D.Solid.BlendNormal");
        }

        textBillboard_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
}

void JustAvoidParticle::Spawn(const Vector3& pos, const Vector3& dir) {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> distLen(2.0f, 4.0f);

    constexpr float kDuration = 0.5f;

    for (auto* bb : billboards_) {
        if (!bb) continue;

        auto* tr = bb->GetComponent3D<Transform3D>();
        if (tr) {
            tr->SetTranslate(pos);
        }

        if (auto* mat = bb->GetComponent3D<Material3D>()) {
            auto c = mat->GetColor();
            c.w = 1.0f;
            mat->SetColor(c);
        }

        const Vector3 d = MakeRandomDirectionNear(dir);
        const float len = distLen(rng);
        const Vector3 to{ pos.x + d.x * len, pos.y + d.y * len, pos.z + d.z * len };

        std::vector<MovementController::MoveEntry> moves;
        moves.push_back(MovementController::MoveEntry{
            pos,
            to,
            kDuration,
            [](Vector3 a, Vector3 b, float t) { return EaseOutCubic(a, b, t); }
        });

        if (auto* mc = bb->GetComponent3D<MovementController>()) {
            mc->SetMoves(std::move(moves));
            mc->Start();
        } else {
            bb->RegisterComponent<MovementController>(std::move(moves));
            if (auto* mc2 = bb->GetComponent3D<MovementController>()) {
                mc2->Start();
            }
        }
    }

    // text billboard motion (pos.y + 0.5 -> pos.y + 2.5)
    if (textBillboard_) {
        const Vector3 from{ pos.x, pos.y + 0.5f, pos.z };
        const Vector3 to{ pos.x, pos.y + 2.5f, pos.z };

        if (auto* tr = textBillboard_->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(from);
        }
        if (auto* mat = textBillboard_->GetComponent3D<Material3D>()) {
            auto c = mat->GetColor();
            c.w = 1.0f;
            mat->SetColor(c);
        }

        std::vector<MovementController::MoveEntry> moves;
        moves.push_back(MovementController::MoveEntry{
            from,
            to,
            kDuration,
            [](Vector3 a, Vector3 b, float t) { return EaseOutCubic(a, b, t); }
        });

        if (auto* mc = textBillboard_->GetComponent3D<MovementController>()) {
            mc->SetMoves(std::move(moves));
            mc->Start();
        } else {
            textBillboard_->RegisterComponent<MovementController>(std::move(moves));
            if (auto* mc2 = textBillboard_->GetComponent3D<MovementController>()) {
                mc2->Start();
            }
        }
    }
}

void JustAvoidParticle::Update() {
    for (auto* bb : billboards_) {
        if (!bb) continue;
        auto* mc = bb->GetComponent3D<MovementController>();
        if (!mc) continue;

        if (!mc->IsMoving()) {
            if (auto* tr = bb->GetComponent3D<Transform3D>()) {
                const auto p = tr->GetTranslate();
                if (p != kHiddenPos) {
                    tr->SetTranslate(kHiddenPos);
                }
            }
            if (auto* mat = bb->GetComponent3D<Material3D>()) {
                auto c = mat->GetColor();
                c.w = 0.0f;
                mat->SetColor(c);
            }
        }
    }

    if (textBillboard_) {
        auto* mc = textBillboard_->GetComponent3D<MovementController>();
        if (mc && !mc->IsMoving()) {
            if (auto* tr = textBillboard_->GetComponent3D<Transform3D>()) {
                const auto p = tr->GetTranslate();
                if (p != kHiddenPos) {
                    tr->SetTranslate(kHiddenPos);
                }
            }
            if (auto* mat = textBillboard_->GetComponent3D<Material3D>()) {
                auto c = mat->GetColor();
                c.w = 0.0f;
                mat->SetColor(c);
            }
        }
    }
}

} // namespace KashipanEngine
