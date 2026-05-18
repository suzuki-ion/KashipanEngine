#include "Scene/Components/Letterbox.h"

namespace KashipanEngine {

void Letterbox::Initialize() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;

    auto *sceneDefaultVariables = ctx->GetComponent<SceneDefaultVariables>();
    screenBuffer2D_ = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer2D() : nullptr;
    if (!screenBuffer2D_) return;

    {
        auto rect = std::make_unique<Rect>();
        rect->SetName("LetterboxTop");
        rect->SetUniqueBatchKey();
        if (auto *mat = rect->GetComponent2D<Material2D>("Material2D")) {
            mat->SetColor(color_);
        }
        if (auto *tr = rect->GetComponent2D<Transform2D>("Transform2D")) {
            tr->SetScale(Vector3(0.0f, 0.0f, 1.0f));
        }
        rect->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
        topRect_ = rect.get();
        ctx->AddObject2D(std::move(rect));
    }

    {
        auto rect = std::make_unique<Rect>();
        rect->SetName("LetterboxBottom");
        rect->SetUniqueBatchKey();
        if (auto *mat = rect->GetComponent2D<Material2D>("Material2D")) {
            mat->SetColor(color_);
        }
        if (auto *tr = rect->GetComponent2D<Transform2D>("Transform2D")) {
            tr->SetScale(Vector3(0.0f, 0.0f, 1.0f));
        }
        rect->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
        bottomRect_ = rect.get();
        ctx->AddObject2D(std::move(rect));
    }

    UpdateTransforms();
}

void Letterbox::Finalize() {
    screenBuffer2D_ = nullptr;
    topRect_ = nullptr;
    bottomRect_ = nullptr;
}

void Letterbox::Update() {
    UpdateTransforms();
}

void Letterbox::SetThickness(float thickness) {
    thickness_ = std::max(0.0f, thickness);
    UpdateTransforms();
}

void Letterbox::SetColor(const Vector4 &color) {
    color_ = color;
    ApplyColor();
}

void Letterbox::ApplyColor() {
    if (topRect_) {
        if (auto *mat = topRect_->GetComponent2D<Material2D>("Material2D")) {
            mat->SetColor(color_);
        }
    }
    if (bottomRect_) {
        if (auto *mat = bottomRect_->GetComponent2D<Material2D>("Material2D")) {
            mat->SetColor(color_);
        }
    }
}

void Letterbox::UpdateTransforms() {
    if (!screenBuffer2D_) return;
    const float w = static_cast<float>(screenBuffer2D_->GetWidth());
    const float h = static_cast<float>(screenBuffer2D_->GetHeight());
    const float t = std::min(thickness_, h * 0.5f);

    if (topRect_) {
        if (auto *tr = topRect_->GetComponent2D<Transform2D>("Transform2D")) {
            tr->SetTranslate(Vector3(w * 0.5f, h - t * 0.5f, 0.0f));
            tr->SetScale(Vector3(w, t, 1.0f));
        }
    }

    if (bottomRect_) {
        if (auto *tr = bottomRect_->GetComponent2D<Transform2D>("Transform2D")) {
            tr->SetTranslate(Vector3(w * 0.5f, t * 0.5f, 0.0f));
            tr->SetScale(Vector3(w, t, 1.0f));
        }
    }
}

} // namespace KashipanEngine
