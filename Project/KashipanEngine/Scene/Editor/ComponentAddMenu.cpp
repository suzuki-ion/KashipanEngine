#include "ComponentAddMenu.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <map>
#include <unordered_map>
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace ComponentAddMenu {

namespace {

/// @brief カテゴリツリーのノード
struct CategoryNode {
    std::map<std::string, CategoryNode> children;
    std::vector<std::string> types;
};

bool ShowNode(const CategoryNode &node, std::string &outSelectedType) {
    bool selected = false;
    for (const auto &pair : node.children) {
        if (ImGui::BeginMenu(pair.first.c_str())) {
            selected |= ShowNode(pair.second, outSelectedType);
            ImGui::EndMenu();
        }
    }
    for (const auto &type : node.types) {
        if (ImGui::MenuItem(type.c_str())) {
            outSelectedType = type;
            selected = true;
        }
    }
    return selected;
}

/// @brief カテゴリ階層を"Render/Light"のような表示用文字列にする
std::string JoinCategory(const std::vector<std::string> &categories) {
    std::string result;
    for (const auto &category : categories) {
        if (!result.empty()) result += '/';
        result += category;
    }
    return result;
}

} // namespace

bool Show(const std::vector<std::string> &types,
    const std::function<const std::vector<std::string> &(const std::string &)> &getCategory,
    std::string &outSelectedType) {
    // 検索ボックス。呼び出し元のポップアップ毎に状態を持ち、ポップアップを開き直す度に空へ戻す
    static std::unordered_map<ImGuiID, ImGuiTextFilter> sFilters;
    ImGuiTextFilter &filter = sFilters[ImGui::GetID("##ComponentSearch")];
    const bool appearing = ImGui::IsWindowAppearing();
    if (appearing) {
        filter.Clear();
        // 開いた直後にそのまま文字を打ち込めるようにする
        ImGui::SetKeyboardFocusHere();
    }
    ImGuiCustom::SearchFilterBox("##ComponentSearch", filter, TranslationC("editor.component.add.search"),
        ImGui::GetFontSize() * 16.0f);
    const bool enterPressed = ImGui::IsItemFocused() && !appearing &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));

    if (filter.IsActive()) {
        // 検索中はカテゴリ階層を無視して、型名またはカテゴリ名が一致した型を平坦なリストで表示する
        ImGui::Separator();
        bool anyMatched = false;
        bool selected = false;
        for (const auto &type : types) {
            const std::string categoryPath = JoinCategory(getCategory(type));
            if (!filter.PassFilter(type.c_str()) && !filter.PassFilter(categoryPath.c_str())) continue;
            // Enterで最初に一致した型を追加する
            if (enterPressed && !anyMatched) {
                outSelectedType = type;
                selected = true;
                ImGui::CloseCurrentPopup();
            }
            anyMatched = true;
            if (ImGui::MenuItem(type.c_str(), categoryPath.empty() ? nullptr : categoryPath.c_str())) {
                outSelectedType = type;
                selected = true;
            }
        }
        if (!anyMatched) {
            ImGui::TextDisabled("%s", TranslationC("editor.component.add.search.noresults"));
        }
        return selected;
    }
    ImGui::Separator();

    // カテゴリ階層からツリーを構築する（カテゴリ無しはルート直下）
    CategoryNode root;
    for (const auto &type : types) {
        CategoryNode *node = &root;
        for (const auto &category : getCategory(type)) {
            node = &node->children[category];
        }
        node->types.push_back(type);
    }

    return ShowNode(root, outSelectedType);
}

} // namespace ComponentAddMenu
} // namespace KashipanEngine

#endif // USE_IMGUI
