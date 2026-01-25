#pragma once
#include <KashipanEngine.h>
<<<<<<< HEAD:Project/Application/Scenes/Components/AttackBase.h
#include "Objects/Components/MovementController.h"
#include "Objects/Components/AlwaysRotate.h"
#include "Scene/Components/SceneDefaultVariables.h"

=======
>>>>>>> TD2_3:Application/Scenes/Components/AttackBase.h
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include "Objects/Components/MovementController.h"

namespace KashipanEngine {

class AttackBase : public ISceneComponent {
public:
    AttackBase(const std::string &type, Object3DBase *mover = nullptr, [[maybe_unused]] ScreenBuffer *screenBuffer = nullptr)
        : ISceneComponent(type, 1), mover_(mover) {}
    ~AttackBase() override = default;

    void SetMover(Object3DBase *mover) { mover_ = mover; }
    Object3DBase *GetMover() const { return mover_; }

    void SetScreenBuffer(ScreenBuffer * /*screenBuffer*/) { /* no-op: use SceneDefaultVariables */ }
    ScreenBuffer *GetScreenBuffer() const { return sceneDefault_ ? sceneDefault_->GetScreenBuffer3D() : nullptr; }

    // 攻撃開始処理（外部から呼ばれる）
    void Attack() { AttackStartInitialize(); }

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        sceneDefault_ = ctx->GetComponent<SceneDefaultVariables>();
    }
    void Finalize() override {
        for (auto *obj : spawnedObjects_) {
            if (obj) {
                if (auto *ctx = GetOwnerContext()) {
                    ctx->RemoveObject3D(obj);
                }
            }
        }
        for (auto *pl : pointLights_) {
            if (pl) {
                if (auto *ctx = GetOwnerContext()) {
                    ctx->RemoveObject3D(pl);
                }
            }
        }

        spawnedObjects_.clear();
        pointLights_.clear();
    }

    void Update() override;

    void SetSpawnWithPointLight(bool v) { spawnWithPointLight_ = v; }

protected:
    using MoveEntry = MovementController::MoveEntry;

    // 攻撃開始時の初期化（オブジェクト生成等）
    virtual void AttackStartInitialize() = 0;

    // 共通 Move: (x,32,z) -> (x,1,z) を 1s で EaseOutBack
    static MoveEntry MakeCommonSpawnDropMove(const Vector3 &spawnPos) {
        MoveEntry e;
        e.from = Vector3{ spawnPos.x, 32.0f, spawnPos.z };
        e.to = Vector3{ spawnPos.x, 1.0f, spawnPos.z };
        e.duration = 1.0f;
        e.easing = [](Vector3 a, Vector3 b, float t) { return EaseOutBack(a, b, t); };
        return e;
    }

    // 共通 Move: 最终位置から y=32へ戻す 1s EaseInBack
    static MoveEntry MakeCommonFinalRiseMove(const Vector3 &fromPos) {
        MoveEntry e;
        e.from = fromPos;
        e.to = Vector3{fromPos.x, 32.0f, fromPos.z};
        e.duration = 1.0f;
        e.easing = [](Vector3 a, Vector3 b, float t) { return EaseInBack(a, b, t); };
        return e;
    }

    // XZ 平面の Plane3D を生成し、mover の子として登録、MovementController を付与して moves をセットして Start する
    Plane3D *SpawnXZPlaneWithMoves(const std::string &name, const Vector3 &spawnPos, const std::vector<MoveEntry> &extraMoves);

    std::vector<Object3DBase *> spawnedObjects_;

    bool spawnWithPointLight_ = false;
    std::vector<PointLight *> pointLights_;

private:
    Object3DBase *mover_ = nullptr;
    SceneDefaultVariables *sceneDefault_ = nullptr;
};

} // namespace KashipanEngine