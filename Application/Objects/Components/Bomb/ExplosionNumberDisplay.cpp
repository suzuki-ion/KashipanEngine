#include "ExplosionNumberDisplay.h"
#include <algorithm>

namespace KashipanEngine {

ExplosionNumberDisplay::ExplosionNumberDisplay()
    : ISceneComponent("ExplosionNumberDisplay") {
}

void ExplosionNumberDisplay::Initialize() {
    activeNumbers_.clear();
}

void ExplosionNumberDisplay::Update() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    const float dt = GetDeltaTime();

    // 数字の更新と寿命管理
    activeNumbers_.erase(
        std::remove_if(activeNumbers_.begin(), activeNumbers_.end(),
            [this, dt, ctx](NumberDisplayInfo& info) {
                info.elapsedTime += dt;

                // 経過割合（0..1）
                float t = 0.0f;
                if (displayLifetime_ > 0.0f) {
                    t = info.elapsedTime / displayLifetime_;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                } else {
                    t = 1.0f;
                }

                // フェードアウト効果: スケールを徐々に大きくしつつ透明に
                if (info.object) {
                    if (auto* tr = info.object->GetComponent3D<Transform3D>()) {
                        // スケールを徐々に大きくする
                        float currentScale = info.initialScale * (1.0f + t * 0.5f);
                        tr->SetScale(Vector3(currentScale, currentScale, currentScale));
                    }

                    // Y座標を上昇させる
                    if (auto* tr = info.object->GetComponent3D<Transform3D>()) {
                        Vector3 pos = info.position;
                        pos.y += t * yOffset_;
                        tr->SetTranslate(pos);
                    }
                }

                // 寿命を超えた数字を削除
                if (info.elapsedTime >= displayLifetime_) {
                    if (info.object) {
                        ctx->RemoveObject3D(info.object);
                    }
                    return true;
                }

                return false;
            }),
        activeNumbers_.end()
    );
}

void ExplosionNumberDisplay::SpawnNumber(const Vector3& position, int count) {
    auto* ctx = GetOwnerContext();
    if (!ctx || !screenBuffer_ || count <= 0) {
        return;
    }

    // 数字に対応するモデルファイル名を取得
    std::string modelFileName;
    switch (count) {
    case 1:
        modelFileName = "100.obj";
        break;
    case 2:
        modelFileName = "300.obj";
        break;
    case 3:
        modelFileName = "900.obj";
        break;
    default:
        modelFileName = "1500.obj"; // 4体以上
        break;
    }

    // モデルを作成
    auto modelData = ModelManager::GetModelDataFromFileName(modelFileName);
    auto numberModel = std::make_unique<Model>(modelData);
    numberModel->SetName("Number_" + std::to_string(activeNumbers_.size()));

    // Transform設定
    Vector3 displayPos = position;
    displayPos.y += 0.5f; // 少し上に表示
    if (auto* tr = numberModel->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(displayPos);
        tr->SetScale(Vector3(numberScale_, numberScale_, numberScale_));
		tr->SetRotate(Vector3(0.3f, 0.0f, 0.0f));
    }

    // Material設定
    if (auto* mat = numberModel->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // モデルのポインタを保存
    Model* modelPtr = numberModel.get();

    // レンダラーにアタッチ
    if (screenBuffer_) {
        numberModel->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        //numberModel->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // シーンに追加
    ctx->AddObject3D(std::move(numberModel));

    // 数字情報を登録
    NumberDisplayInfo info;
    info.object = modelPtr;
    info.elapsedTime = 0.0f;
    info.position = displayPos;
    info.number = count;
    info.initialScale = numberScale_;
    activeNumbers_.push_back(info);
}

#if defined(USE_IMGUI)
void ExplosionNumberDisplay::ShowImGui() {
    ImGui::Text("Active Numbers: %d", static_cast<int>(activeNumbers_.size()));
    ImGui::DragFloat("Display Lifetime", &displayLifetime_, 0.01f, 0.1f, 5.0f);
    ImGui::DragFloat("Number Scale", &numberScale_, 0.01f, 0.1f, 5.0f);
    ImGui::DragFloat("Y Offset", &yOffset_, 0.01f, 0.0f, 5.0f);

    if (ImGui::TreeNode("Active Numbers List")) {
        for (size_t i = 0; i < activeNumbers_.size(); ++i) {
            const auto& info = activeNumbers_[i];
            ImGui::Text("Number %zu: Count=%d Time=%.2f/%.2f",
                i, info.number, info.elapsedTime, displayLifetime_);
        }
        ImGui::TreePop();
    }
}
#endif

} // namespace KashipanEngine
