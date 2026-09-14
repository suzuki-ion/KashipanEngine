#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector3.h"
#if defined(USE_IMGUI)
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief CameraController(3D用)の移動範囲制限（Camera Bounds Zone）で使う「グループ」「矩形」の定義を持つコンポーネント
/// @details CameraBoundsZone2Dの3D版。矩形は軸並行の直方体（コンポーネント所有オブジェクトのローカル空間）。
///          フットプリント（XZ平面上の広がり）はシーンビューポート上でのギズモ編集を想定しているが、
///          高さ方向（Y、min.y/max.y）はインスペクターの数値入力のみで編集する
///          （3D空間中の全頂点をドラッグ編集対象にするのはコストが高いための意図的なスコープ縮小）。
///          判定・クランプのロジックはCameraBoundsZone2Dと同一（矩形ごとにクランプし最も近い結果を採用）
class CameraBoundsZone3D final : public IObjectComponent {
public:
    /// @brief 1つの矩形（軸並行の直方体、所有オブジェクトのローカル空間）
    struct Rect {
        Vector3 min{ -100.0f, -100.0f, -100.0f };
        Vector3 max{ 100.0f, 100.0f, 100.0f };
    };
    /// @brief 複数の矩形から成るグループ
    struct Group {
        std::vector<Rect> rects;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(CameraBoundsZone3D, 1, )
    COMPONENT_CATEGORY("Render")
    ~CameraBoundsZone3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraBoundsZone3D>();
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
    bool IsPointInGroup(size_t groupIndex, const Vector3 &localPoint) const {
        if (groupIndex >= groups_.size()) return false;
        for (const auto &rect : groups_[groupIndex].rects) {
            if (localPoint.x >= rect.min.x && localPoint.x <= rect.max.x &&
                localPoint.y >= rect.min.y && localPoint.y <= rect.max.y &&
                localPoint.z >= rect.min.z && localPoint.z <= rect.max.z) {
                return true;
            }
        }
        return false;
    }

    /// @brief 点を含むグループを検索する（ローカル空間、見つからなければ-1）
    int FindGroupContaining(const Vector3 &localPoint) const {
        for (size_t i = 0; i < groups_.size(); ++i) {
            if (IsPointInGroup(i, localPoint)) return static_cast<int>(i);
        }
        return -1;
    }

    /// @brief 指定グループの矩形群に沿って点をクランプする（ローカル空間）
    /// @param insetMin 各矩形のmin側を内側へ縮める量（座標基準クランプ時はゼロベクトル）
    /// @param insetMax 各矩形のmax側を内側へ縮める量（座標基準クランプ時はゼロベクトル。
    ///        3Dの表示範囲基準クランプはXZのみに適用し、Yは常にゼロを渡す想定）
    /// @details 矩形ごとにクランプし、元の点との距離が最も近い結果を採用する（凹形状の範囲に対応するため）
    Vector3 ClampToGroup(size_t groupIndex, const Vector3 &localPoint,
        const Vector3 &insetMin = Vector3(0.0f, 0.0f, 0.0f), const Vector3 &insetMax = Vector3(0.0f, 0.0f, 0.0f)) const {
        if (groupIndex >= groups_.size()) return localPoint;
        const auto &rects = groups_[groupIndex].rects;
        if (rects.empty()) return localPoint;

        bool hasBest = false;
        Vector3 best{ 0.0f, 0.0f, 0.0f };
        float bestDistSq = 0.0f;
        for (const auto &rect : rects) {
            const Vector3 clamped(
                ClampAxis(localPoint.x, rect.min.x, rect.max.x, insetMin.x, insetMax.x),
                ClampAxis(localPoint.y, rect.min.y, rect.max.y, insetMin.y, insetMax.y),
                ClampAxis(localPoint.z, rect.min.z, rect.max.z, insetMin.z, insetMax.z)
            );
            const float dx = clamped.x - localPoint.x;
            const float dy = clamped.y - localPoint.y;
            const float dz = clamped.z - localPoint.z;
            const float distSq = dx * dx + dy * dy + dz * dz;
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
        ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.cameraboundszone3d.description"));
        ImGui::Separator();

        int removeGroupIndex = -1;
        for (size_t g = 0; g < groups_.size(); ++g) {
            ImGui::PushID(static_cast<int>(g));
            const std::string groupLabel = std::string(TranslationC("component.cameraboundszone3d.group")) + " " + std::to_string(g);
            if (ImGui::CollapsingHeader(groupLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &rects = groups_[g].rects;
                int removeRectIndex = -1;
                for (size_t r = 0; r < rects.size(); ++r) {
                    ImGui::PushID(static_cast<int>(r));
                    auto &rect = rects[r];
                    const bool isSelected = (editorSelectedGroup_ == static_cast<int>(g) && editorSelectedRect_ == static_cast<int>(r));
                    if (ImGui::Selectable(TranslationC("component.cameraboundszone3d.rect"), isSelected)) {
                        editorSelectedGroup_ = static_cast<int>(g);
                        editorSelectedRect_ = static_cast<int>(r);
                    }
                    ImGui::DragFloat3(TranslationLabel("component.cameraboundszone3d.rect_min"), &rect.min.x, 1.0f);
                    ImGui::DragFloat3(TranslationLabel("component.cameraboundszone3d.rect_max"), &rect.max.x, 1.0f);
                    if (ImGui::Button(TranslationLabel("component.cameraboundszone3d.remove_rect"))) removeRectIndex = static_cast<int>(r);
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
                if (ImGui::Button(TranslationLabel("component.cameraboundszone3d.add_rect"))) AddRect(g);
                ImGui::SameLine();
                if (ImGui::Button(TranslationLabel("component.cameraboundszone3d.remove_group"))) removeGroupIndex = static_cast<int>(g);
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
        if (ImGui::Button(TranslationLabel("component.cameraboundszone3d.add_group"))) AddGroup();
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
                        if (rectJson.contains("min")) rect.min = FromJSON<Vector3>(rectJson["min"]);
                        if (rectJson.contains("max")) rect.max = FromJSON<Vector3>(rectJson["max"]);
                        group.rects.push_back(rect);
                    }
                }
                groups_.push_back(group);
            }
        }
        return true;
    }

private:
    /// @brief 1軸分のクランプ（insetMin/insetMaxで内側へ縮めた範囲が反転する場合は矩形中心へフォールバックする）
    static float ClampAxis(float value, float min, float max, float insetMinAmount, float insetMaxAmount) {
        float insetMin = min + insetMinAmount;
        float insetMax = max - insetMaxAmount;
        if (insetMin > insetMax) {
            insetMin = insetMax = (min + max) * 0.5f;
        }
        return std::clamp(value, insetMin, insetMax);
    }

    std::vector<Group> groups_;

#if defined(USE_IMGUI)
    /// @brief エディターのビューポートギズモが現在編集対象としている矩形（非シリアライズ、無選択は-1）
    int editorSelectedGroup_ = -1;
    int editorSelectedRect_ = -1;
#endif
};

REGISTER_COMPONENT_OBJECT(CameraBoundsZone3D)

} // namespace KashipanEngine
