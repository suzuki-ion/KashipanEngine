#include "EngineLogoScene.h"

#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Utilities/MathUtils/Easings.h"
#include "Scenes/Components/SceneFade.h"

namespace KashipanEngine {

EngineLogoScene::EngineLogoScene(const std::string &nextSceneName)
    : SceneBase("EngineLogoScene") {
    SetNextSceneName(nextSceneName);
}

EngineLogoScene::~EngineLogoScene() = default;

void EngineLogoScene::Initialize() {
    // キャッシュされたシーンデフォルト変数を取得
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    int32_t winW = 1280;
    int32_t winH = 720;
    if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
        winW = sceneDefaultVariables_->GetMainWindow()->GetClientWidth();
        winH = sceneDefaultVariables_->GetMainWindow()->GetClientHeight();
    }

    // 2D描画先 ScreenBuffer を取得
    ScreenBuffer *screenBuffer2D = nullptr;
    if (sceneDefaultVariables_) screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();

    float srcW = 1280.0f;
    float srcH = 720.0f;
    if (screenBuffer2D) {
        srcW = static_cast<float>(screenBuffer2D->GetWidth());
        srcH = static_cast<float>(screenBuffer2D->GetHeight());
    }

    // 背景用 Rect を生成
    {
        auto bg = std::make_unique<Rect>();
        bg->SetName("EngineLogoBackground");
        bg->SetUniqueBatchKey();
        // Transform および Material は既に存在する前提なので登録しない
        // 背景色: F7C052 -> (247,192,82) / 255
        const Vector4 bgColor(static_cast<float>(247) / 255.0f,
                              static_cast<float>(192) / 255.0f,
                              static_cast<float>(82) / 255.0f,
                              1.0f);
        if (auto m = bg->GetComponent2D<Material2D>("Material2D")) {
            m->SetColor(bgColor);
        }

        // Transform の設定
        if (auto t = bg->GetComponent2D<Transform2D>("Transform2D")) {
            t->SetTranslate(Vector3(static_cast<float>(winW) * 0.5f, static_cast<float>(winH) * 0.5f, 0.0f));
            t->SetScale(Vector3(static_cast<float>(winW), static_cast<float>(winH), 1.0f));
        }

        // ScreenBuffer にアタッチ (パイプライン名: Object2D.DoubleSidedCulling.BlendNormal)
        if (screenBuffer2D) {
            bg->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        // シーンに追加
        AddObject2D(std::move(bg));
    }

    // スプライト共通設定（位置は中央を基準に少しずらす）
    const float cx = static_cast<float>(srcW) * 0.5f;

    // ロゴギア: X=center, Y=height*2/3
    {
        const auto handle = GetTextureManager()->GetTextureFromFileName("engineLogoGear.png");
        auto sp = std::make_unique<Sprite>();
        sp->SetName("EngineLogoGear");
        sp->SetUniqueBatchKey();
        sp->SetAnchorPoint(0.5f, 0.5f);
        // Transform/Material は既に登録されている前提
        if (auto t = sp->GetComponent2D<Transform2D>("Transform2D")) {
            t->SetScale(Vector3(0.0f, 0.0f, 1.0f));
            t->SetTranslate(Vector3(cx, static_cast<float>(srcH) * 0.5f, 0.0f));
        }
        if (auto m = sp->GetComponent2D<Material2D>("Material2D")) {
            m->SetTexture(handle);
        }

        if (screenBuffer2D) {
            sp->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        logoGearSprite_ = sp.get();
        AddObject2D(std::move(sp));
    }

    // ロゴパン: X=center, Y=height*2/3
    {
        const auto handle = GetTextureManager()->GetTextureFromFileName("engineLogoBread.png");
        auto sp = std::make_unique<Sprite>();
        sp->SetName("EngineLogoBread");
        sp->SetUniqueBatchKey();
        sp->SetAnchorPoint(0.5f, 0.5f);
        if (auto t = sp->GetComponent2D<Transform2D>("Transform2D")) {
            t->SetScale(Vector3(0.0f, 0.0f, 1.0f));
            t->SetTranslate(Vector3(cx, static_cast<float>(srcH) * 0.5f, 0.0f));
        }
        if (auto m = sp->GetComponent2D<Material2D>("Material2D")) {
            m->SetTexture(handle);
        }

        if (screenBuffer2D) {
            sp->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        logoBreadSprite_ = sp.get();
        AddObject2D(std::move(sp));
    }

    // ロゴテキスト: X=center, Y=height*1/3
    {
        const auto handle = GetTextureManager()->GetTextureFromFileName("engineLogoText.png");
        auto sp = std::make_unique<Sprite>();
        sp->SetName("EngineLogoText");
        sp->SetUniqueBatchKey();
        sp->SetAnchorPoint(0.5f, 0.5f);
        if (auto t = sp->GetComponent2D<Transform2D>("Transform2D")) {
            t->SetScale(Vector3(1024.0f, 256.0f, 1.0f));
            t->SetTranslate(Vector3(cx, static_cast<float>(srcH) * 0.2f, 0.0f));
        }
        if (auto m = sp->GetComponent2D<Material2D>("Material2D")) {
            m->SetTexture(handle);
            m->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.0f));
        }

        if (screenBuffer2D) {
            sp->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        logoTextSprite_ = sp.get();
        AddObject2D(std::move(sp));
    }

    // アニメーションで使用する矩形オブジェクトを事前生成して保持
    {
        auto ar = std::make_unique<Rect>();
        ar->SetName("EngineLogo_AnimRect");
        ar->SetUniqueBatchKey();
        if (auto t = ar->GetComponent2D<Transform2D>("Transform2D")) {
            // ロゴテキストと同じY、Xは中心
            t->SetTranslate(Vector3(cx, static_cast<float>(srcH) * 0.2f, 0.0f));
            // Yスケールはテキストと一致させる（初期は小さめ）
            t->SetScale(Vector3(0.0f, 256.0f, 1.0f));
        }
        if (auto m = ar->GetComponent2D<Material2D>("Material2D")) {
            const Vector4 arColor(static_cast<float>(101) / 255.0f,
                                  static_cast<float>(64) / 255.0f,
                                  static_cast<float>(24) / 255.0f,
                                  1.0f);
            m->SetColor(arColor);
        }

        if (screenBuffer2D) {
            ar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        // シーンに追加しつつポインタを保持
        animationRect_ = ar.get();
        AddObject2D(std::move(ar));
    }

    // フェード用コンポーネントを追加
    AddSceneComponent(std::make_unique<SceneFade>());

    // 初期設定: フェード色や時間が必要ならここで設定可能
    if (auto *fade = GetSceneComponent<SceneFade>()) {
        fade->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        fade->SetDuration(1.0f);
    }

    elapsedTime_ = 0.0f;
}

void EngineLogoScene::OnUpdate() {
    const float dt = GetDeltaTime();
    if (dt <= 0.0f || dt > 1.0f) return;
    elapsedTime_ += dt;
    const float t = elapsedTime_ - animationStartOffset_;
    if (t < 0.0f) return;

    // logoGear: t 0.0 - 0.5 scale 0->512 EaseOutBack, rotation 0->360 EaseOutCubic
    if (logoGearSprite_) {
        if (auto tcomp = logoGearSprite_->GetComponent2D<Transform2D>("Transform2D")) {
            const float nt1 = Normalize01(t, 0.0f, 0.5f);
            const float s = EaseOutBack(0.0f, 512.0f, nt1);
            tcomp->SetScale(Vector3(s, s, 1.0f));
            const float nt2 = Normalize01(t, 0.0f, 0.5f);
            const float rot = EaseOutCubic(0.0f, 360.0f, nt2);
            tcomp->SetRotate(Vector3(0.0f, 0.0f, rot * (3.14159265f / 180.0f)));
        }
    }

    // logoBread: t 0.25 - 0.75 scale 0->512 EaseOutCubic
    if (logoBreadSprite_) {
        if (auto tcomp = logoBreadSprite_->GetComponent2D<Transform2D>("Transform2D")) {
            const float nt = Normalize01(t, 0.25f, 0.75f);
            const float s = EaseOutCubic(0.0f, 512.0f, nt);
            tcomp->SetScale(Vector3(s, s, 1.0f));
        }
    }

    // logoText: at t == 0.75 instantly set color alpha to 1.0
    if (logoTextSprite_) {
        if (auto mcomp = logoTextSprite_->GetComponent2D<Material2D>("Material2D")) {
            if (elapsedTime_ >= (animationStartOffset_ + 0.75f)) {
                mcomp->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
    }

    // animationRect: 0.5-0.75 expand across text left->right, 0.75-1.0 shrink left->right
    if (animationRect_ && logoTextSprite_) {
        auto rectT = animationRect_->GetComponent2D<Transform2D>("Transform2D");
        auto textT = logoTextSprite_->GetComponent2D<Transform2D>("Transform2D");
        if (rectT && textT) {
            // compute text left/right based on text center and scale.x
            const float textCenterX = textT->GetTranslate().x;
            const float textWidth = textT->GetScale().x;
            const float left = textCenterX - textWidth * 0.5f;
            const float right = textCenterX + textWidth * 0.5f;

            if (t >= 0.5f && t <= 0.75f) {
                const float nt = Normalize01(t, 0.5f, 0.75f);
                const float p = EaseOutCubic(0.0f, 1.0f, nt);
                // position from left to right
                const float centerX = Lerp(left, textCenterX, p);
                const float width = Lerp(0.0f, textWidth, p);
                rectT->SetTranslate(Vector3(centerX, rectT->GetTranslate().y, 0.0f));
                rectT->SetScale(Vector3(width, rectT->GetScale().y, 1.0f));
            } else if (t >= 0.75f) {
                const float nt = Normalize01(t, 0.75f, 1.0f);
                const float p = EaseOutCubic(0.0f, 1.0f, nt);
                // shrink from full width back to 0 but keep center moving left->right
                const float centerX = Lerp(textCenterX, right, p);
                const float width = Lerp(textWidth, 0.0f, p);
                rectT->SetTranslate(Vector3(centerX, rectT->GetTranslate().y, 0.0f));
                rectT->SetScale(Vector3(width, rectT->GetScale().y, 1.0f));
            }
        }
    }

    // フェード開始タイミング: t >= 2.0 でフェードアウトを開始
    if (elapsedTime_ >= (animationStartOffset_ + 2.0f) && prevElapsedTime_ < (animationStartOffset_ + 2.0f)) {
        if (auto *fade = GetSceneComponent<SceneFade>()) {
            fade->PlayOut();
        }
    }

    // フェード完了で次シーンへ
    if (!GetNextSceneName().empty()) {
        if (auto *fade = GetSceneComponent<SceneFade>()) {
            if (fade->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    prevElapsedTime_ = elapsedTime_;
}

} // namespace KashipanEngine
