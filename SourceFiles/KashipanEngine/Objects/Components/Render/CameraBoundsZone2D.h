#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector2.h"
#if defined(USE_IMGUI)
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief CameraController2Dの移動範囲制限（Camera Bounds Zone）で使う「グループ」「矩形」の定義を持つコンポーネント
/// @details 複数の「グループ」を持ち、各グループは複数の矩形（軸並行、コンポーネント所有オブジェクトの
///          ローカル空間で定義）から成る。CameraController2D側は、追従ターゲットが今どのグループに
///          含まれているかを毎フレーム判定し、そのグループの矩形群に沿ってカメラ位置をクランプする
///          （矩形ごとにクランプした結果のうち最も近いものを採用することで、複数矩形からなる
///          凹形状の範囲にも対応する）。このコンポーネント自体は判定・クランプの計算のみを提供し、
///          実際のクランプ適用はCameraController2D::Update()側で行う
class CameraBoundsZone2D final : public IObjectComponent {
public:
    /// @brief 1つの矩形（軸並行、所有オブジェクトのローカル空間）
    struct Rect {
        Vector2 min{ -100.0f, -100.0f };
        Vector2 max{ 100.0f, 100.0f };
    };
    /// @brief 複数の矩形から成るグループ
    struct Group {
        std::vector<Rect> rects;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(CameraBoundsZone2D, 1, )
    COMPONENT_CATEGORY("Render")
    ~CameraBoundsZone2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraBoundsZone2D>();
        ptr->groups_ = groups_;
        return ptr;
    }

    //==================================================
    // グループ・矩形の編集
    //==================================================

    std::vector<Group> &GetGroups() noexcept { return groups_; }
    const std::vector<Group> &GetGroups() const noexcept { return groups_; }
    size_t GetGroupCount() const noexcept { return groups_.size(); }
    void AddGroup() { groups_.push_back(Group{}); }
    void RemoveGroup(size_t groupIndex) {
        if (groupIndex < groups_.size()) groups_.erase(groups_.begin() + groupIndex);
    }
    void AddRect(size_t groupIndex) {
        if (groupIndex < groups_.size()) groups_[groupIndex].rects.push_back(Rect{});
    }
    void RemoveRect(size_t groupIndex, size_t rectIndex) {
        if (groupIndex >= groups_.size()) return;
        auto &rects = groups_[groupIndex].rects;
        if (rectIndex < rects.size()) rects.erase(rects.begin() + rectIndex);
    }

    //==================================================
    // 判定・クランプ
    //==================================================

    /// @brief 指定グループの矩形群のいずれかに点が含まれるか（ローカル空間）
    bool IsPointInGroup(size_t groupIndex, const Vector2 &localPoint) const {
        if (groupIndex >= groups_.size()) return false;
        for (const auto &rect : groups_[groupIndex].rects) {
            if (localPoint.x >= rect.min.x && localPoint.x <= rect.max.x &&
                localPoint.y >= rect.min.y && localPoint.y <= rect.max.y) {
                return true;
            }
        }
        return false;
    }

    /// @brief 点を含むグループを検索する（ローカル空間、見つからなければ-1）
    int FindGroupContaining(const Vector2 &localPoint) const {
        for (size_t i = 0; i < groups_.size(); ++i) {
            if (IsPointInGroup(i, localPoint)) return static_cast<int>(i);
        }
        return -1;
    }

    /// @brief 指定グループの矩形群に沿って点をクランプする（ローカル空間）
    /// @param insetMin 各矩形のmin側を内側へ縮める量（座標基準クランプ時はゼロベクトル）
    /// @param insetMax 各矩形のmax側を内側へ縮める量（座標基準クランプ時はゼロベクトル）
    /// @details 矩形ごとにクランプし、元の点との距離が最も近い結果を採用する（凹形状の範囲に対応するため）。
    ///          insetMin/insetMaxを別々に指定できるのは、呼び出し側の座標系が対称とは限らないため
    ///          （例: Camera2Dのtranslateは表示範囲の左上角を表すため、表示範囲基準クランプではmax側のみ
    ///          表示サイズぶん縮める必要がある。詳細はCameraController2D::Update参照）
    Vector2 ClampToGroup(size_t groupIndex, const Vector2 &localPoint,
        const Vector2 &insetMin = Vector2(0.0f, 0.0f), const Vector2 &insetMax = Vector2(0.0f, 0.0f)) const {
        if (groupIndex >= groups_.size()) return localPoint;
        const auto &rects = groups_[groupIndex].rects;
        if (rects.empty()) return localPoint;

        bool hasBest = false;
        Vector2 best{ 0.0f, 0.0f };
        float bestDistSq = 0.0f;
        for (const auto &rect : rects) {
            float insetMinX = rect.min.x + insetMin.x;
            float insetMaxX = rect.max.x - insetMax.x;
            if (insetMinX > insetMaxX) insetMinX = insetMaxX = (rect.min.x + rect.max.x) * 0.5f;
            float insetMinY = rect.min.y + insetMin.y;
            float insetMaxY = rect.max.y - insetMax.y;
            if (insetMinY > insetMaxY) insetMinY = insetMaxY = (rect.min.y + rect.max.y) * 0.5f;

            const Vector2 clamped(
                std::clamp(localPoint.x, insetMinX, insetMaxX),
                std::clamp(localPoint.y, insetMinY, insetMaxY)
            );
            const float dx = clamped.x - localPoint.x;
            const float dy = clamped.y - localPoint.y;
            const float distSq = dx * dx + dy * dy;
            if (!hasBest || distSq < bestDistSq) {
                hasBest = true;
                bestDistSq = distSq;
                best = clamped;
            }
        }
        return best;
    }

#if defined(USE_IMGUI)
    /// @brief エディターのビューポートギズモが現在編集対象としている矩形（無選択は-1）
    void GetEditorSelection(int &outGroupIndex, int &outRectIndex) const {
        outGroupIndex = editorSelectedGroup_;
        outRectIndex = editorSelectedRect_;
    }
    void SetEditorSelection(int groupIndex, int rectIndex) {
        editorSelectedGroup_ = groupIndex;
        editorSelectedRect_ = rectIndex;
    }

    void ShowImGui() override {
        ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.cameraboundszone2d.description"));
        ImGui::Separator();

        int removeGroupIndex = -1;
        for (size_t g = 0; g < groups_.size(); ++g) {
            ImGui::PushID(static_cast<int>(g));
            const std::string groupLabel = std::string(TranslationC("component.cameraboundszone2d.group")) + " " + std::to_string(g);
            if (ImGui::CollapsingHeader(groupLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &rects = groups_[g].rects;
                int removeRectIndex = -1;
                for (size_t r = 0; r < rects.size(); ++r) {
                    ImGui::PushID(static_cast<int>(r));
                    auto &rect = rects[r];
                    const bool isSelected = (editorSelectedGroup_ == static_cast<int>(g) && editorSelectedRect_ == static_cast<int>(r));
                    if (ImGui::Selectable(TranslationC("component.cameraboundszone2d.rect"), isSelected)) {
                        editorSelectedGroup_ = static_cast<int>(g);
                        editorSelectedRect_ = static_cast<int>(r);
                    }
                    ImGui::DragFloat2(TranslationLabel("component.cameraboundszone2d.rect_min"), &rect.min.x, 1.0f);
                    ImGui::DragFloat2(TranslationLabel("component.cameraboundszone2d.rect_max"), &rect.max.x, 1.0f);
                    if (ImGui::Button(TranslationLabel("component.cameraboundszone2d.remove_rect"))) removeRectIndex = static_cast<int>(r);
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeRectIndex >= 0) {
                    RemoveRect(g, static_cast<size_t>(removeRectIndex));
                    if (editorSelectedGroup_ == static_cast<int>(g) && editorSelectedRect_ == removeRectIndex) {
                        editorSelectedGroup_ = -1;
                        editorSelectedRect_ = -1;
                    }
                }
                if (ImGui::Button(TranslationLabel("component.cameraboundszone2d.add_rect"))) AddRect(g);
                ImGui::SameLine();
                if (ImGui::Button(TranslationLabel("component.cameraboundszone2d.remove_group"))) removeGroupIndex = static_cast<int>(g);
            }
            ImGui::PopID();
        }
        if (removeGroupIndex >= 0) {
            RemoveGroup(static_cast<size_t>(removeGroupIndex));
            if (editorSelectedGroup_ == removeGroupIndex) {
                editorSelectedGroup_ = -1;
                editorSelectedRect_ = -1;
            }
        }
        if (ImGui::Button(TranslationLabel("component.cameraboundszone2d.add_group"))) AddGroup();
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        JSON groupsJson = JSON::array();
        for (const auto &group : groups_) {
            JSON groupJson = JSON::object();
            JSON rectsJson = JSON::array();
            for (const auto &rect : group.rects) {
                JSON rectJson = JSON::object();
                rectJson["min"] = ToJSON(rect.min);
                rectJson["max"] = ToJSON(rect.max);
                rectsJson.push_back(rectJson);
            }
            groupJson["rects"] = rectsJson;
            groupsJson.push_back(groupJson);
        }
        json["groups"] = groupsJson;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        groups_.clear();
        if (json.contains("groups")) {
            for (const auto &groupJson : json["groups"]) {
                Group group;
                if (groupJson.contains("rects")) {
                    for (const auto &rectJson : groupJson["rects"]) {
                        Rect rect;
                        if (rectJson.contains("min")) rect.min = FromJSON<Vector2>(rectJson["min"]);
                        if (rectJson.contains("max")) rect.max = FromJSON<Vector2>(rectJson["max"]);
                        group.rects.push_back(rect);
                    }
                }
                groups_.push_back(group);
            }
        }
        return true;
    }

private:
    std::vector<Group> groups_;

#if defined(USE_IMGUI)
    /// @brief エディターのビューポートギズモが現在編集対象としている矩形（非シリアライズ、無選択は-1）
    int editorSelectedGroup_ = -1;
    int editorSelectedRect_ = -1;
#endif
};

REGISTER_COMPONENT_OBJECT(CameraBoundsZone2D)

} // namespace KashipanEngine
