#include "ComponentAddMenu.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <map>

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

} // namespace

bool Show(const std::vector<std::string> &types,
    const std::function<const std::vector<std::string> &(const std::string &)> &getCategory,
    std::string &outSelectedType) {
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
