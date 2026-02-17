#include "Scenes/Components/PlayerHealthModelUI.h"
#include "Objects/Components/Health.h"
#include <algorithm>

namespace KashipanEngine {

    void PlayerHealthModelUI::SetHealth(Health* health) {
        health_ = health;
        maxHpAtBind_ = health_ ? std::max(0, health_->GetHp()) : 0;
        EnsureModels();
        UpdateModelColors();
    }

    void PlayerHealthModelUI::SetTransform(Transform3D* transform) {
        parentTransform_ = transform;
        
        // 既存のモデルにペアレントを設定
        for (auto* obj : hpObject_) {
            if (obj) {
                if (auto* tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(parentTransform_);
                }
            }
        }

        for (auto* obj : hpOutLineObject_) {
            if (obj) {
                if (auto* tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(parentTransform_);
                }
            }
        }
    }

    void PlayerHealthModelUI::EnsureModels() {
        auto* ctx = GetOwnerContext();
        if (!ctx) return;

        if (static_cast<int>(hpObject_.size()) == maxHpAtBind_) return;

        for (auto* obj : hpObject_) {
            if (obj) ctx->RemoveObject3D(obj);
        }
        hpObject_.clear();

        for (auto* obj : hpOutLineObject_) {
            if (obj) ctx->RemoveObject3D(obj);
        }
        hpOutLineObject_.clear();

        auto hpHandle = ModelManager::GetModelDataFromFileName("playerHp.obj");
        auto outLineHandle = ModelManager::GetModelDataFromFileName("playerHpOutline.obj");

        for (int i = 0; i < maxHpAtBind_; ++i) {
            auto hpObj = std::make_unique<Model>(hpHandle);
            hpObj->SetUniqueBatchKey();
            hpObj->SetName(std::string("PlayerHealthModelUI_HP_") + std::to_string(i));

            if (screenBuffer_) {
                hpObj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }

            if (auto* mat = hpObj->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 1.0f,0.0f,0.0f,1.0f });
            }

            if (auto* tr = hpObj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3{-2.9f + (0.3f * i), 1.47f, 5.0f});
                tr->SetScale(Vector3{0.25f});
                // ペアレントを設定
                if (parentTransform_) {
                    tr->SetParentTransform(parentTransform_);
                }
            }

            hpObj->RegisterComponent<BPMScaling>(Vector3{ 0.25f,0.25f,0.25f }, Vector3(0.275f, 0.235f, 0.25f),EaseType::EaseInExpo);

            auto* rawHp = hpObj.get();
            hpObject_.push_back(rawHp);
            ctx->AddObject3D(std::move(hpObj));

            auto outlineObj = std::make_unique<Model>(outLineHandle);
            outlineObj->SetUniqueBatchKey();
            outlineObj->SetName(std::string("PlayerHealthModelUI_Outline_") + std::to_string(i));

            if (screenBuffer_) {
                outlineObj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }

            if (auto* mat = outlineObj->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 0.0f,0.0f,0.0f,1.0f });
            }

            if (auto* tr = outlineObj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3{ -2.9f + (0.3f * i), 1.47f, 5.0f });
                tr->SetScale(Vector3{ 0.23f });
                // ペアレントを設定
                if (parentTransform_) {
                    tr->SetParentTransform(parentTransform_);
                }
            }

            auto* rawOutline = outlineObj.get();
            hpOutLineObject_.push_back(rawOutline);
            ctx->AddObject3D(std::move(outlineObj));
        }
    }

    void PlayerHealthModelUI::UpdateModelColors() {
        const int hp = health_ ? std::max(0, health_->GetHp()) : 0;

        if (isPause_) {
            for (int i = 0; i < static_cast<int>(hpObject_.size()); ++i) {
                auto* obj = hpObject_[i];
                if (!obj) continue;

                auto* mat = obj->GetComponent3D<Material3D>();
                if (!mat) continue;

                mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
            }

            for (int i = 0; i < static_cast<int>(hpOutLineObject_.size()); ++i) {
                auto* obj = hpOutLineObject_[i];
                if (!obj) continue;

                auto* mat = obj->GetComponent3D<Material3D>();
                if (!mat) continue;

                mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
            }
        } else {
            for (int i = 0; i < static_cast<int>(hpObject_.size()); ++i) {
                auto* obj = hpObject_[i];
                if (!obj) continue;

                auto* scaling = obj->GetComponent3D<BPMScaling>();
                if (scaling) {
                    scaling->SetBPMProgress(bpmProgress_);
                }

                auto* mat = obj->GetComponent3D<Material3D>();
                if (!mat) continue;

                if (i < hp) {
                    mat->SetColor(Vector4{ 1.0f, 0.0f, 0.0f, 1.0f });
                } else {
                    mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
                }
            }

            for (int i = 0; i < static_cast<int>(hpOutLineObject_.size()); ++i) {
                auto* obj = hpOutLineObject_[i];
                if (!obj) continue;
                auto* mat = obj->GetComponent3D<Material3D>();
                if (!mat) continue;

                if (i < hp) {
                    mat->SetColor(Vector4{ 0.2f, 0.2f, 0.2f, 1.0f });
                } else {
                    mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
                }
            }
        }
    }

    void PlayerHealthModelUI::Update() {
        EnsureModels();
        UpdateModelColors();
    }

} // namespace KashipanEngine
