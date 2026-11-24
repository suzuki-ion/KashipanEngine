#pragma once
#include "Objects/ObjectComponentHeader.h"
#include <cmath>
namespace KashipanEngine {
/// シーン編集用のメタデータのみを保持する。起動時の再生ではシーンコンポーネントを生成しない。
class LoadingAnimationSection final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(LoadingAnimationSection, 0xFF,
        ADD_MEMBER_VARIABLE(opacity_);
    )
    COMPONENT_CATEGORY("Animation")
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto p = std::make_unique<LoadingAnimationSection>();
        p->section_ = section_; p->endMode_ = endMode_; p->seconds_ = seconds_;
        p->completion_ = completion_; p->opacity_ = opacity_; return p;
    }
protected:
    JSON SaveToJson() const override {
        return JSON{{"section", section_}, {"endMode", endMode_}, {"seconds", seconds_},
            {"completion", completion_}, {"opacity", opacity_}};
    }
    bool LoadFromJson(const JSON &j) override {
        if (!j.is_object()) return false;
        section_ = j.value("section", 0); endMode_ = j.value("endMode", 1);
        seconds_ = j.contains("seconds") && j["seconds"].is_number() ? j["seconds"].get<float>() : 0;
        completion_ = j.value("completion", 1); opacity_ = j.value("opacity", 1.f); return true;
    }
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Combo("区間", &section_, "開始\0完了待ち\0終了\0共通\0");
        if (section_ != 3) {
            ImGui::Combo("終了位置", &endMode_, "秒数を指定\0最長アニメーションから自動\0");
            if (endMode_ == 0) {
                ImGui::InputFloat("秒数", &seconds_);
                if (!std::isfinite(seconds_) || seconds_ <= 0) ImGui::TextWrapped("無効な秒数です。この区間はスキップします。");
            }
            if (section_ == 1) ImGui::Combo("読み込み完了時", &completion_, "すぐ終了区間へ\0現在の周期が終わるまで待つ\0");
        }
        ImGui::SliderFloat("グループ不透明度", &opacity_, 0.f, 1.f);
        ImGui::TextWrapped("子のSpriteRendererだけを描画します。KeyFrameAnimatorの自動再生を有効にしてください。座標は1920x1080、左下原点です。");
    }
#endif
private:
    int section_ = 0, endMode_ = 1, completion_ = 1;
    float seconds_ = 1, opacity_ = 1;
};
REGISTER_COMPONENT_OBJECT(LoadingAnimationSection)
}
