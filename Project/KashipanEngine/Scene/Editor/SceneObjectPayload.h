#pragma once
#ifdef USE_IMGUI

namespace KashipanEngine {

class EmptyObject;

/// @brief ヒエラルキーからのオブジェクトD&Dペイロード型名
inline constexpr const char *kSceneObjectDragDropType = "DND_OBJECT";

/// @brief ヒエラルキーからのオブジェクトD&Dペイロード
/// @details ImGui::AcceptDragDropPayload(kSceneObjectDragDropType) で受け取り、
///          object からドラッグされたシーンオブジェクトを取得する。
struct SceneObjectDragDropPayload {
    EmptyObject *object = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
