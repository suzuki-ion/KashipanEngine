#pragma once

namespace KashipanEngine::Dialogs {

/// @brief メッセージダイアログを表示する
/// @param title ダイアログのタイトル
/// @param message 表示するメッセージ
/// @param isError エラーメッセージかどうか
/// @return ユーザーがOKを押したらtrue、キャンセルを押したらfalse
bool ShowMessageDialog(const char *title, const char *message, bool isError = false);

} // namespace KashipanEngine::Dialogs