#include "ExplosionManager.h"
#include "BombManager.h"
#include "Objects/Components/Enemy/EnemyManager.h"
#include <algorithm>

namespace KashipanEngine {

ExplosionManager::ExplosionManager()
    : ISceneComponent("ExplosionManager") {
}

void ExplosionManager::Initialize() {
    // 初期化処理
    activeExplosions_.clear();
}

void ExplosionManager::Update() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    const float dt = GetDeltaTime();

    // 爆発とボムの衝突をチェック
    CheckExplosionBombCollisions();

    // 爆発の更新と寿命管理（同時にスケールを徐々に小さくする）
    activeExplosions_.erase(
        std::remove_if(activeExplosions_.begin(), activeExplosions_.end(),
            [this, dt, ctx](ExplosionInfo& explosion) {
                explosion.elapsedTime += dt;

                // 経過割合（0..1）
                float t = 0.0f;
                if (explosionLifetime_ > 0.0f) {
                    t = explosion.elapsedTime / explosionLifetime_;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                } else {
                    t = 1.0f;
                }

                // 線形補間でスケールを縮小（初期 scale -> 0）
                Vector3 startScale((size_ * 2.0f) + 0.5f, 0.5f, 0.5f);
                Vector3 endScale(0.0f, 0.0f, 0.0f);
                Vector3 newScale = Vector3::Lerp(startScale, endScale, t);

                if (explosion.object) {
                    if (auto* tr = explosion.object->GetComponent3D<Transform3D>()) {
                        tr->SetScale(newScale);
                    }
                }

                // 線形補間でスケールを縮小（初期 scale -> 0）
                Vector3 startScale2(0.5f, 0.5f, (size_ * 2.0f) + 0.5f);
                Vector3 endScale2(0.0f, 0.0f, 0.0f);
                Vector3 newScale2 = Vector3::Lerp(startScale2, endScale2, t);

                if (explosion.object2) {
                    if (auto* tr = explosion.object2->GetComponent3D<Transform3D>()) {
                        tr->SetScale(newScale2);
                    }
                }

                // 寿命を超えた爆発を削除
                if (explosion.elapsedTime >= explosionLifetime_) {
                    if (explosion.object) {
                        ctx->RemoveObject3D(explosion.object);
                    }

                    if (explosion.object2) {
                        ctx->RemoveObject3D(explosion.object2);
                    }

                    return true;
                }

                return false;
            }),
        activeExplosions_.end()
    );
}

void ExplosionManager::SpawnExplosion(const Vector3& position) {
    auto* ctx = GetOwnerContext();
    if (!ctx || !screenBuffer_) {
        return;
    }

    // 爆発オブジェクトを作成（BombManagerと同じパターン）
    auto modelData = ModelManager::GetModelDataFromFileName("Explosion.obj");
    auto explosion = std::make_unique<Model>(modelData);
    explosion->SetName("Explosion_" + std::to_string(activeExplosions_.size()));
    auto explosion2 = std::make_unique<Model>(modelData);
    explosion2->SetName("Explosion2_" + std::to_string(activeExplosions_.size()));

    // Transform設定
    if (auto* tr = explosion->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3((size_ * 2.0f) + 0.5f, 0.5f, 0.5f));
    }

    // Transform設定
    if (auto* tr = explosion2->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3(0.5f, 0.5f, (size_ * 2.0f) + 0.5f));
    }

    // Material設定
    if (auto* mat = explosion->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(1.0f, 0.5f, 0.0f, 1.0f)); // オレンジ色の爆発エフェクト
    }

    if (auto* mat = explosion2->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(1.0f, 0.5f, 0.0f, 1.0f)); // オレンジ色の爆発エフェクト
    }

    // 爆発オブジェクトのポインタを保存（move前）
    Model* explosionPtr = explosion.get();
    Model* explosion2Ptr = explosion2.get();

    if (collider_ && collider_->GetCollider()) {
        ColliderInfo3D info;
        Math::AABB aabb;
        aabb.min = Vector3{ -size_ * 2.0f, -0.5f, -0.5f };
        aabb.max = Vector3{ +size_ * 2.0f, +0.5f, +0.5f };
        info.shape = aabb;
        info.attribute.set(2);

        // ラムダで爆発オブジェクトポインタをキャプチャ
        info.onCollisionEnter = [this, explosionPtr](const HitInfo3D& hitInfo) {
            // EnemyManagerが設定されていなければ何もしない
            if (!enemyManager_) return;

            // 敵との衝突をEnemyManagerに通知
            enemyManager_->OnExplosionHit(hitInfo.otherObject);
        };

        explosion->RegisterComponent<Collision3D>(collider_->GetCollider(), info);
    }

    if (collider2_ && collider2_->GetCollider()) {
        ColliderInfo3D info;
        Math::AABB aabb;
        aabb.min = Vector3{ -0.5f, -0.5f, -size_ * 2.0f };
        aabb.max = Vector3{ +0.5f, +0.5f, +size_ * 2.0f };
        info.shape = aabb;
        info.attribute.set(2);

        // ラムダで爆発オブジェクトポインタをキャプチャ
        info.onCollisionEnter = [this, explosion2Ptr](const HitInfo3D& hitInfo) {
            // EnemyManagerが設定されていなければ何もしない
            if (!enemyManager_) return;

            // 敵との衝突をEnemyManagerに通知
            enemyManager_->OnExplosionHit(hitInfo.otherObject);
        };

        explosion2->RegisterComponent<Collision3D>(collider2_->GetCollider(), info);
    }

    // レンダラーにアタッチ
    if (screenBuffer_) {
        explosion->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
		explosion2->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        explosion->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
		explosion2->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // 爆発情報を登録
    ExplosionInfo info;
    info.object = explosionPtr;
	info.object2 = explosion2Ptr;
    info.elapsedTime = 0.0f;
    info.position = position;
    activeExplosions_.push_back(info);

    // シーンに追加
    ctx->AddObject3D(std::move(explosion));
	ctx->AddObject3D(std::move(explosion2));

    // 爆発音再生（オプション）
    auto soundHandle = AudioManager::GetSoundHandleFromFileName("explosion.mp3");
    if (soundHandle != AudioManager::kInvalidSoundHandle) {
        AudioManager::Play(soundHandle, 0.1f, 0.0f, false);
    }
}

void ExplosionManager::CheckExplosionBombCollisions() {
    if (!bombManager_) return;

    // 各爆発について、範囲内のボムをチェック
    for (const auto& explosion : activeExplosions_) {
        // 爆発の範囲（十字型の範囲）
        const float explosionRange = size_ * 2.0f;
        
        // BombManagerから全ボムの位置をチェックして起爆
        bombManager_->DetonateBombsInExplosionRange(explosion.position, explosionRange);
    }
}

#if defined(USE_IMGUI)
void ExplosionManager::ShowImGui() {
    ImGui::Text("Active Explosions: %d", GetActiveExplosionCount());
    ImGui::DragFloat("Explosion Scale", &explosionScale_, 0.01f, 0.1f, 5.0f);
    ImGui::DragFloat("Explosion Lifetime", &explosionLifetime_, 0.01f, 0.1f, 10.0f);

    if (ImGui::TreeNode("Active Explosions List")) {
        for (size_t i = 0; i < activeExplosions_.size(); ++i) {
            const auto& explosion = activeExplosions_[i];
            ImGui::Text("Explosion %zu: Pos(%.2f, %.2f, %.2f) Time: %.2f/%.2f",
                i,
                explosion.position.x, explosion.position.y, explosion.position.z,
                explosion.elapsedTime, explosionLifetime_);
        }
        ImGui::TreePop();
    }
}
#endif

} // namespace KashipanEngine
