#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <typeinfo>
#include <utility>
#include <optional>
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Color.h"
#include "Assets/TextureRef.h"
#include "Assets/TextureCubeRef.h"
#include "Assets/TextureManager.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/MyAny.h"
#include "Utilities/Translation.h"

namespace ImGuiCustom {

// ==========================================
// 1. 共通オプション構造体 ＆ 前方宣言
// ==========================================

struct UiOptions {
    bool asSlider = false;         // true: Slider系を使用 / false: Drag系を使用
    float vSpeed = 1.0f;           // Drag時の変化速度
    float vMin = 0.0f;             // 最小値 (0.0fのままなら型ごとのデフォルトを適用)
    float vMax = 0.0f;             // 最大値 (0.0fのままなら型ごとのデフォルトを適用)
    const char *format = nullptr;  // フォーマット (nullptrなら型ごとのデフォルト)
    int flags = 0;                 // ImGuiSliderFlags や ImGuiInputTextFlags 等の兼用フラグ
};

namespace detail {
template <typename T>
inline std::string ToString(const T &val) {
    if constexpr (std::is_same_v<T, std::string>) {
        return val;
    } else if constexpr (std::is_same_v<T, const char *>) {
        return std::string(val);
    } else {
        return std::to_string(val);
    }
}
}

// ==========================================
// 1-2. 文字列の選択コンボ
// ==========================================

/// @brief 文字列を候補リストから選択するコンボボックス
/// @param label ラベル
/// @param value 現在値（変更時に上書きされる）
/// @param items 候補リスト
/// @param allowNone 先頭に「(None)」を表示して空文字を選択可能にする
/// @return 値が変更された場合は true
inline bool SelectString(const char *label, std::string &value, const std::vector<std::string> &items, bool allowNone = false) {
    bool changed = false;
    const char *preview = value.empty() ? "(None)" : value.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        if (allowNone) {
            const bool selected = value.empty();
            if (ImGui::Selectable(KashipanEngine::TranslationLabel("editor.imguicustom.none"), selected) && !selected) {
                value.clear();
                changed = true;
            }
        }
        for (const auto &item : items) {
            const bool selected = (item == value);
            if (ImGui::Selectable(item.c_str(), selected) && !selected) {
                value = item;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ==========================================
// 1-3. テクスチャアセットのサムネイル選択（Unity風ピッカー）
// ==========================================

/// @brief テクスチャアセットをサムネイル付きボタン+ポップアップグリッドから選択するUnity風ピッカー。
///        プレビューのサムネイルボタンをクリックすると候補一覧をサムネイルグリッドで表示する
///        ポップアップが開き、クリックで選択できる（Scene/Editor/AssetsWindow::ShowFileGridと
///        同じ`ImGui::ImageButton`+`TextureListEntry::srvGpuPtr`方式）。Assetsウィンドウからの
///        ドラッグ&ドロップも従来通りプレビューボタンへ受け付ける
/// @param label ラベル（プレビューの右に表示する。ImGui ID分離のため内部でPushIDする）
/// @param assetPath 選択中のアセット相対パス（変更時に上書きされる。空文字列="未選択"）
/// @param candidates 選択候補（呼び出し側でTextureCubeRef用等のフィルタ済みの一覧を渡す）
/// @param allowNone ポップアップ先頭に「(None)」セルを表示して空文字を選択可能にする
/// @param dropFilter 指定時、ドラッグ&ドロップされたアセットパスがtrueを返す場合のみ受け付ける
///        （EditValue(TextureCubeRef&)がキューブマップ以外のドロップを無視するために使う）
/// @return 値が変更された場合は true
inline bool TextureThumbnailPicker(const char *label, std::string &assetPath,
    const std::vector<KashipanEngine::TextureManager::TextureListEntry> &candidates, bool allowNone = true,
    const std::function<bool(const std::string &)> &dropFilter = nullptr) {
    bool changed = false;
    constexpr float kThumbnailSize = 48.0f;
    constexpr float kGridThumbnailSize = 64.0f;
    constexpr float kGridCellSize = 84.0f;

    ImGui::PushID(label);

    // 選択中テクスチャのSRVは候補一覧（呼び出し側で読み込み済みテクスチャから作られる）から探す。
    // 候補に含まれない（フィルタで外れた等）場合は見つからずプレースホルダー表示になる
    const auto *current = assetPath.empty() ? nullptr : [&]() -> const KashipanEngine::TextureManager::TextureListEntry * {
        for (const auto &entry : candidates) {
            if (entry.assetPath == assetPath) return &entry;
        }
        return nullptr;
    }();

    if (current && current->srvGpuPtr != 0) {
        if (ImGui::ImageButton("##preview", static_cast<ImTextureID>(current->srvGpuPtr), ImVec2(kThumbnailSize, kThumbnailSize))) {
            ImGui::OpenPopup("##texPickerPopup");
        }
    } else {
        if (ImGui::Button(assetPath.empty() ? "(None)" : "?", ImVec2(kThumbnailSize, kThumbnailSize))) {
            ImGui::OpenPopup("##texPickerPopup");
        }
    }
    if (ImGui::IsItemHovered() && !assetPath.empty()) {
        ImGui::SetTooltip("%s", assetPath.c_str());
    }
    if (std::string droppedPath; KashipanEngine::AcceptAssetDragDropTarget(KashipanEngine::kTextureAssetDragDropType, droppedPath)) {
        if (!dropFilter || dropFilter(droppedPath)) {
            assetPath = droppedPath;
            changed = true;
        }
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::TextUnformatted(assetPath.empty() ? "(None)" : assetPath.c_str());
    ImGui::EndGroup();

    if (ImGui::BeginPopup("##texPickerPopup")) {
        static char sFilterBuf[128] = "";
        ImGui::SetNextItemWidth(kGridCellSize * 3.0f);
        ImGui::InputTextWithHint("##texPickerFilter", "Search...", sFilterBuf, sizeof(sFilterBuf));
        ImGui::Separator();

        std::string filterLower = sFilterBuf;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const float availWidth = ImGui::GetContentRegionAvail().x;
        const int columns = std::max(1, static_cast<int>(availWidth / kGridCellSize));

        ImGui::BeginChild("##texPickerGrid", ImVec2(0.0f, 320.0f));
        int index = 0;
        if (allowNone) {
            if (ImGui::Button(KashipanEngine::TranslationLabel("editor.imguicustom.none"), ImVec2(kGridThumbnailSize, kGridThumbnailSize))) {
                assetPath.clear();
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ++index;
        }
        for (const auto &entry : candidates) {
            if (!filterLower.empty()) {
                std::string pathLower = entry.assetPath;
                std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (pathLower.find(filterLower) == std::string::npos) continue;
            }
            if (index % columns != 0) ImGui::SameLine();
            ImGui::PushID(entry.assetPath.c_str());
            bool clicked = false;
            if (entry.srvGpuPtr != 0) {
                const bool selected = (entry.assetPath == assetPath);
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                clicked = ImGui::ImageButton("##thumb", static_cast<ImTextureID>(entry.srvGpuPtr), ImVec2(kGridThumbnailSize, kGridThumbnailSize));
                if (selected) ImGui::PopStyleColor();
            } else {
                clicked = ImGui::Button("?", ImVec2(kGridThumbnailSize, kGridThumbnailSize));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%ux%u", entry.assetPath.c_str(), entry.width, entry.height);
            }
            if (clicked) {
                assetPath = entry.assetPath;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ++index;
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

// ==========================================
// 2. EditValue 拡張関数群 (編集用)
// ==========================================

// --- 2-1. 基礎型 ---
inline bool EditValue(const char *label, bool &value, const UiOptions &opts = {}) {
    (void)opts; // boolではオプションは無視
    return ImGui::Checkbox(label, &value);
}

inline bool EditValue(const char *label, int &value, const UiOptions &opts = {}) {
    if (opts.asSlider) {
        int minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0 : static_cast<int>(opts.vMin);
        int maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 100 : static_cast<int>(opts.vMax);
        const char *fmt = opts.format ? opts.format : "%d";
        return ImGui::SliderInt(label, &value, minVal, maxVal, fmt, opts.flags);
    } else {
        const char *fmt = opts.format ? opts.format : "%d";
        return ImGui::DragInt(label, &value, opts.vSpeed, static_cast<int>(opts.vMin), static_cast<int>(opts.vMax), fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, float &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
        float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
        return ImGui::SliderFloat(label, &value, minVal, maxVal, fmt, opts.flags);
    } else {
        return ImGui::DragFloat(label, &value, opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, double &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        double minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0 : static_cast<double>(opts.vMin);
        double maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0 : static_cast<double>(opts.vMax);
        return ImGui::SliderScalar(label, ImGuiDataType_Double, &value, &minVal, &maxVal, fmt, opts.flags);
    } else {
        double minVal = static_cast<double>(opts.vMin);
        double maxVal = static_cast<double>(opts.vMax);
        return ImGui::DragScalar(label, ImGuiDataType_Double, &value, opts.vSpeed, &minVal, &maxVal, fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, std::string &value, const UiOptions &opts = {}) {
    return ImGui::InputText(label, &value, opts.flags);
}

// --- 2-2. カスタム数学型 ---
inline bool EditValue(const char *label, Vector2 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
        float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
        return ImGui::SliderFloat2(label, &value.x, minVal, maxVal, fmt, opts.flags);
    } else {
        return ImGui::DragFloat2(label, &value.x, opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, Vector3 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
        float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
        return ImGui::SliderFloat3(label, &value.x, minVal, maxVal, fmt, opts.flags);
    } else {
        return ImGui::DragFloat3(label, &value.x, opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, Vector4 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
        float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
        return ImGui::SliderFloat4(label, &value.x, minVal, maxVal, fmt, opts.flags);
    } else {
        return ImGui::DragFloat4(label, &value.x, opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags);
    }
}

/// @brief 色編集用（内部データはVector4と同じr,g,b,a）。opts（step/range）はColorEdit4には適用されない
inline bool EditValue(const char *label, Color &value, const UiOptions &opts = {}) {
    (void)opts;
    return ImGui::ColorEdit4(label, &value.r);
}

/// @brief テクスチャ参照編集用。サムネイル付きボタンをクリックすると候補一覧をサムネイルグリッドで
///        表示するポップアップが開く（TextureThumbnailPicker参照）。Assetsウィンドウからの
///        D&Dも受け付ける（MaterialManagerの固定テクスチャスロット選択欄と同じ操作感）。optsは使用しない
inline bool EditValue(const char *label, TextureRef &value, const UiOptions &opts = {}) {
    (void)opts;
    return TextureThumbnailPicker(label, value.assetPath, KashipanEngine::TextureManager::GetLoadedTextureListEntries(), true);
}

/// @brief キューブマップ参照編集用。EditValue(TextureRef&)と同じ操作感だが、候補を
///        isCubemapなテクスチャのみに絞り込み、平面画像を誤って選べないようにする
///        （ドラッグ&ドロップも同様にキューブマップ以外は無視する）
inline bool EditValue(const char *label, TextureCubeRef &value, const UiOptions &opts = {}) {
    (void)opts;
    std::vector<KashipanEngine::TextureManager::TextureListEntry> cubemapEntries;
    for (auto &entry : KashipanEngine::TextureManager::GetLoadedTextureListEntries()) {
        if (entry.isCubemap) cubemapEntries.push_back(std::move(entry));
    }
    auto dropFilter = [&cubemapEntries](const std::string &path) {
        for (const auto &entry : cubemapEntries) {
            if (entry.assetPath == path) return true;
        }
        return false;
    };
    return TextureThumbnailPicker(label, value.assetPath, cubemapEntries, true, dropFilter);
}

inline bool EditValue(const char *label, Quaternion &value, const UiOptions &opts = {}) {
    std::string q_label = std::string(label) + " (X,Y,Z,W)";
    const char *fmt = opts.format ? opts.format : "%.3f";
    if (opts.asSlider) {
        float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
        float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
        return ImGui::SliderFloat4(q_label.c_str(), &value.x, minVal, maxVal, fmt, opts.flags);
    } else {
        return ImGui::DragFloat4(q_label.c_str(), &value.x, opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags);
    }
}

inline bool EditValue(const char *label, Matrix3x3 &value, const UiOptions &opts = {}) {
    bool changed = false;
    ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.desc_1"), label, opts.asSlider ? " (Slider)" : "");
    ImGui::Indent();
    const char *fmt = opts.format ? opts.format : "%.3f";
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        std::string row_label = "[" + std::to_string(i) + "]";
        if (opts.asSlider) {
            float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
            float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
            if (ImGui::SliderFloat3(row_label.c_str(), value.m[i], minVal, maxVal, fmt, opts.flags)) { changed = true; }
        } else {
            if (ImGui::DragFloat3(row_label.c_str(), value.m[i], opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags)) { changed = true; }
        }
        ImGui::PopID();
    }
    ImGui::Unindent();
    return changed;
}

inline bool EditValue(const char *label, Matrix4x4 &value, const UiOptions &opts = {}) {
    bool changed = false;
    ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.desc_2"), label, opts.asSlider ? " (Slider)" : "");
    ImGui::Indent();
    const char *fmt = opts.format ? opts.format : "%.3f";
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        std::string row_label = "[" + std::to_string(i) + "]";
        if (opts.asSlider) {
            float minVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 0.0f : opts.vMin;
            float maxVal = (opts.vMin == 0.0f && opts.vMax == 0.0f) ? 1.0f : opts.vMax;
            if (ImGui::SliderFloat4(row_label.c_str(), value.m[i], minVal, maxVal, fmt, opts.flags)) { changed = true; }
        } else {
            if (ImGui::DragFloat4(row_label.c_str(), value.m[i], opts.vSpeed, opts.vMin, opts.vMax, fmt, opts.flags)) { changed = true; }
        }
        ImGui::PopID();
    }
    ImGui::Unindent();
    return changed;
}

// --- 2-3. コンテナ型 (テンプレート・再帰処理) ---
template <typename T>
bool EditValue(const char *label, std::vector<T> &vec, const UiOptions &opts = {}) {
    bool changed = false;
    std::string node_name = std::string(label) + " [" + std::to_string(vec.size()) + " items]";

    if (ImGui::TreeNode(node_name.c_str())) {
        auto insertIt = vec.end();
        auto eraseIt = vec.end();
        for (size_t i = 0; i < vec.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button("+")) { insertIt = vec.begin() + i; }
            ImGui::SameLine();
            if (ImGui::Button("-")) { eraseIt = vec.begin() + i; }
            ImGui::SameLine();
            std::string item_label = "[" + std::to_string(i) + "]";
            if (EditValue(item_label.c_str(), vec[i], opts)) { changed = true; }
            ImGui::PopID();
        }
        if (insertIt != vec.end()) { vec.insert(insertIt, T{}); changed = true; }
        if (eraseIt != vec.end()) { vec.erase(eraseIt); changed = true; }
        if (ImGui::Button("+")) { vec.emplace_back(); changed = true; }
        ImGui::TreePop();
    }
    return changed;
}

template <typename K, typename V>
bool EditValue(const char *label, std::unordered_map<K, V> &map, const UiOptions &opts = {}) {
    bool changed = false;
    std::string node_name = std::string(label) + " [" + std::to_string(map.size()) + " pairs]";

    if (ImGui::TreeNode(node_name.c_str())) {
        int id = 0;
        std::optional<std::pair<K, K>> pending_rename = std::nullopt;
        std::optional<K> pending_erase = std::nullopt;

        for (auto &kv : map) {
            ImGui::PushID(id++);
            if (ImGui::Button("-", ImVec2(20, 0))) { pending_erase = kv.first; }
            ImGui::SameLine();

            K temp_key = kv.first;
            ImGui::SetNextItemWidth(100.0f);
            if (EditValue("##key", temp_key)) {
                if (temp_key != kv.first && map.find(temp_key) == map.end()) {
                    pending_rename = { kv.first, temp_key };
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s", KashipanEngine::TranslationC("editor.imguicustom.desc_3"));
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (EditValue("##value", kv.second, opts)) { changed = true; }
            ImGui::PopID();
        }

        if (pending_erase.has_value()) { map.erase(pending_erase.value()); changed = true; }
        if (pending_rename.has_value()) {
            auto node = map.extract(pending_rename->value().first);
            node.key() = pending_rename->value().second;
            map.insert(std::move(node));
            changed = true;
        }

        ImGui::Separator();
        static std::unordered_map<ImGuiID, K> new_key_buffers;
        ImGuiID current_id = ImGui::GetID(label);
        K &new_key_buffer = new_key_buffers[current_id];

        if (ImGui::Button("+", ImVec2(20, 0))) {
            if (map.find(new_key_buffer) == map.end()) {
                map[new_key_buffer] = V{};
                new_key_buffer = K{};
                changed = true;
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        EditValue("##new_key", new_key_buffer);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", KashipanEngine::TranslationC("editor.imguicustom.new_key"));

        ImGui::TreePop();
    }
    return changed;
}

// --- 2-4. 動的型 (std::any) ---
inline bool EditValue(const char *label, std::any &value, const UiOptions &opts = {}) {
    if (!value.has_value()) { ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_empty"), label); return false; }

    const auto &type = value.type();
    bool changed = false;

    if (type == typeid(int)) {
        auto v = std::any_cast<int>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(float)) {
        auto v = std::any_cast<float>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(double)) {
        auto v = std::any_cast<double>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(bool)) {
        auto v = std::any_cast<bool>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(std::string)) {
        auto v = std::any_cast<std::string>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Vector2)) {
        auto v = std::any_cast<Vector2>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Vector3)) {
        auto v = std::any_cast<Vector3>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Vector4)) {
        auto v = std::any_cast<Vector4>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Quaternion)) {
        auto v = std::any_cast<Quaternion>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Matrix3x3)) {
        auto v = std::any_cast<Matrix3x3>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else if (type == typeid(Matrix4x4)) {
        auto v = std::any_cast<Matrix4x4>(value);
        if (EditValue(label, v, opts)) { value = v; changed = true; }
    } else {
        ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_unsupported_any_type_s"), label, type.name());
    }
    return changed;
}

// --- 2-5. 動的型 (MyAny) ---
// SceneVariablesMenu（シーン変数）とMaterialManager（マテリアルの追加パラメータ）で共通利用する
inline bool EditValue(const char *label, MyAny &value, const UiOptions &opts = {}) {
    if (value.IsEmpty()) { ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_empty"), label); return false; }

    switch (value.GetTypeInfo().GetBaseType()) {
    case ValueType::Bool:
        if (auto *v = value.AnyCastPtr<bool>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Int32:
        if (auto *v = value.AnyCastPtr<int>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Float:
        if (auto *v = value.AnyCastPtr<float>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Double:
        if (auto *v = value.AnyCastPtr<double>()) return EditValue(label, *v, opts);
        break;
    case ValueType::String:
        if (auto *v = value.AnyCastPtr<std::string>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Vector2:
        if (auto *v = value.AnyCastPtr<Vector2>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Vector3:
        if (auto *v = value.AnyCastPtr<Vector3>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Vector4:
        if (auto *v = value.AnyCastPtr<Vector4>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Color:
        if (auto *v = value.AnyCastPtr<Color>()) return EditValue(label, *v, opts);
        break;
    case ValueType::TextureRef:
        if (auto *v = value.AnyCastPtr<TextureRef>()) return EditValue(label, *v, opts);
        break;
    case ValueType::TextureCubeRef:
        if (auto *v = value.AnyCastPtr<TextureCubeRef>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Quaternion:
        if (auto *v = value.AnyCastPtr<Quaternion>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Matrix3x3:
        if (auto *v = value.AnyCastPtr<Matrix3x3>()) return EditValue(label, *v, opts);
        break;
    case ValueType::Matrix4x4:
        if (auto *v = value.AnyCastPtr<Matrix4x4>()) return EditValue(label, *v, opts);
        break;
    default:
        break;
    }
    ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_unsupported_any_type_s"), label, value.GetTypeInfo().ToString().c_str());
    return false;
}


// ==========================================
// 3. ShowValue 拡張関数群 (表示専用)
// ==========================================

// --- 3-1. 基礎型 ---
inline void ShowValue(const char *label, const bool &value, const UiOptions &opts = {}) {
    (void)opts;
    ImGui::LabelText(label, value ? "true" : "false");
}
inline void ShowValue(const char *label, const int &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%d";
    ImGui::LabelText(label, fmt, value);
}
inline void ShowValue(const char *label, const float &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(label, fmt, value);
}
inline void ShowValue(const char *label, const double &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(label, fmt, value);
}
inline void ShowValue(const char *label, const std::string &value, const UiOptions &opts = {}) {
    (void)opts;
    ImGui::LabelText(label, "%s", value.c_str());
}

// --- 3-2. カスタム数学型 ---
inline void ShowValue(const char *label, const Vector2 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(label, (std::string(fmt) + ", " + fmt).c_str(), value.x, value.y);
}
inline void ShowValue(const char *label, const Vector3 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(label, (std::string(fmt) + ", " + fmt + ", " + fmt).c_str(), value.x, value.y, value.z);
}
inline void ShowValue(const char *label, const Vector4 &value, const UiOptions &opts = {}) {
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(label, (std::string(fmt) + ", " + fmt + ", " + fmt + ", " + fmt).c_str(), value.x, value.y, value.z, value.w);
}
inline void ShowValue(const char *label, const Quaternion &value, const UiOptions &opts = {}) {
    std::string q_label = std::string(label) + " (Quaternion)";
    const char *fmt = opts.format ? opts.format : "%.3f";
    ImGui::LabelText(q_label.c_str(), (std::string(fmt) + ", " + fmt + ", " + fmt + ", " + fmt).c_str(), value.x, value.y, value.z, value.w);
}
inline void ShowValue(const char *label, const Matrix3x3 &value, const UiOptions &opts = {}) {
    ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_readonly"), label);
    ImGui::Indent();
    const char *fmt = opts.format ? opts.format : "%.3f";
    std::string composite_fmt = std::string(fmt) + ", " + fmt + ", " + fmt;
    for (int i = 0; i < 3; ++i) {
        std::string row_label = "[" + std::to_string(i) + "]";
        ImGui::LabelText(row_label.c_str(), composite_fmt.c_str(), value.m[i][0], value.m[i][1], value.m[i][2]);
    }
    ImGui::Unindent();
}
inline void ShowValue(const char *label, const Matrix4x4 &value, const UiOptions &opts = {}) {
    ImGui::Text(KashipanEngine::TranslationC("editor.imguicustom.s_readonly"), label);
    ImGui::Indent();
    const char *fmt = opts.format ? opts.format : "%.3f";
    std::string composite_fmt = std::string(fmt) + ", " + fmt + ", " + fmt + ", " + fmt;
    for (int i = 0; i < 4; ++i) {
        std::string row_label = "[" + std::to_string(i) + "]";
        ImGui::LabelText(row_label.c_str(), composite_fmt.c_str(), value.m[i][0], value.m[i][1], value.m[i][2], value.m[i][3]);
    }
    ImGui::Unindent();
}

// --- 3-3. コンテナ型 (テンプレート・再帰処理) ---
template <typename T>
void ShowValue(const char *label, const std::vector<T> &vec, const UiOptions &opts = {}) {
    std::string node_name = std::string(label) + " [" + std::to_string(vec.size()) + " items] (ReadOnly)";
    if (ImGui::TreeNode(node_name.c_str())) {
        for (size_t i = 0; i < vec.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            std::string item_label = "[" + std::to_string(i) + "]";
            ShowValue(item_label.c_str(), vec[i], opts);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

template <typename K, typename V>
void ShowValue(const char *label, const std::unordered_map<K, V> &map, const UiOptions &opts = {}) {
    std::string node_name = std::string(label) + " [" + std::to_string(map.size()) + " pairs] (ReadOnly)";
    if (ImGui::TreeNode(node_name.c_str())) {
        int id = 0;
        for (const auto &kv : map) {
            ImGui::PushID(id++);
            std::string key_label = detail::ToString(kv.first);
            ShowValue(key_label.c_str(), kv.second, opts);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

// --- 3-4. 動的型 (std::any) ---
inline void ShowValue(const char *label, const std::any &value, const UiOptions &opts = {}) {
    if (!value.has_value()) { ImGui::LabelText(label, "[Empty]"); return; }

    const auto &type = value.type();

    if (type == typeid(int)) { ShowValue(label, std::any_cast<int>(value), opts); } else if (type == typeid(float)) { ShowValue(label, std::any_cast<float>(value), opts); } else if (type == typeid(double)) { ShowValue(label, std::any_cast<double>(value), opts); } else if (type == typeid(bool)) { ShowValue(label, std::any_cast<bool>(value), opts); } else if (type == typeid(std::string)) { ShowValue(label, std::any_cast<std::string>(value), opts); } else if (type == typeid(Vector2)) { ShowValue(label, std::any_cast<Vector2>(value), opts); } else if (type == typeid(Vector3)) { ShowValue(label, std::any_cast<Vector3>(value), opts); } else if (type == typeid(Vector4)) { ShowValue(label, std::any_cast<Vector4>(value), opts); } else if (type == typeid(Quaternion)) { ShowValue(label, std::any_cast<Quaternion>(value), opts); } else if (type == typeid(Matrix3x3)) { ShowValue(label, std::any_cast<Matrix3x3>(value), opts); } else if (type == typeid(Matrix4x4)) { ShowValue(label, std::any_cast<Matrix4x4>(value), opts); } else {
        ImGui::LabelText(label, "[Unsupported Any Type: %s]", type.name());
    }
}

} // namespace ImGuiCustom

#endif // USE_IMGUI