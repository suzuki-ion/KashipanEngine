#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include "Utilities/FileIO.h"

namespace KashipanEngine {

/// @brief エディターUIの状態（開閉状況など）をexe再起動をまたいで保存する設定ストア
/// @details 値の変更時に即座にJSONファイルへ保存される。
class EditorSettings final {
public:
    /// @brief bool値の取得
    static bool GetBool(const std::string &key, bool defaultValue) {
        EnsureLoaded();
        auto it = sData_.find(key);
        if (it == sData_.end() || !it->is_boolean()) return defaultValue;
        return it->get<bool>();
    }

    /// @brief bool値の設定（変更があった場合のみ保存する）
    static void SetBool(const std::string &key, bool value) {
        EnsureLoaded();
        auto it = sData_.find(key);
        if (it != sData_.end() && it->is_boolean() && it->get<bool>() == value) return;
        sData_[key] = value;
        SaveJSON(sData_, kFilePath);
    }

    /// @brief 開閉状態を保存するツリーノード（デフォルトは開いた状態）
    /// @details 開閉状態は key で保存され、再起動後も維持される。
    static bool PersistentTreeNode(const char *label, const std::string &key, bool defaultOpen = true) {
        const bool stored = GetBool(key, defaultOpen);
        ImGui::SetNextItemOpen(stored, ImGuiCond_Once);
        const bool open = ImGui::TreeNode(label);
        if (open != stored) SetBool(key, open);
        return open;
    }

    /// @brief 開閉状態を保存する折りたたみヘッダー（デフォルトは開いた状態）
    static bool PersistentCollapsingHeader(const char *label, const std::string &key, bool defaultOpen = true) {
        const bool stored = GetBool(key, defaultOpen);
        ImGui::SetNextItemOpen(stored, ImGuiCond_Once);
        const bool open = ImGui::CollapsingHeader(label);
        if (open != stored) SetBool(key, open);
        return open;
    }

private:
    static constexpr const char *kFilePath = "Assets/KashipanEngine/EditorSettings.json";

    static void EnsureLoaded() {
        if (sLoaded_) return;
        sData_ = LoadJSON(kFilePath);
        if (!sData_.is_object()) sData_ = JSON::object();
        sLoaded_ = true;
    }

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
