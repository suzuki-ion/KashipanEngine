#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief コンポーネント追加メニューをカテゴリごとのツリー構造で表示するヘルパー
namespace ComponentAddMenu {

/// @brief カテゴリ階層のツリーメニューを表示する
/// @param types 登録済みコンポーネント型名のリスト
/// @param getCategory 型名からカテゴリ階層を取得する関数
/// @param outSelectedType 選択された型名の出力先
/// @return 型が選択された場合は true
bool Show(const std::vector<std::string> &types,
    const std::function<const std::vector<std::string> &(const std::string &)> &getCategory,
    std::string &outSelectedType);

} // namespace ComponentAddMenu

} // namespace KashipanEngine

#endif // USE_IMGUI
