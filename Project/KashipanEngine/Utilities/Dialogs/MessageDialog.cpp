#include "MessageDialog.h"
#include <Windows.h>

namespace KashipanEngine::Dialogs {

bool ShowMessageDialog(const char *title, const char *message, bool isError) {
    // 戻り値でOK/キャンセルを判定する仕様（ヘッダのドキュメント参照）のため、
    // ボタンが1つしかないMB_OKではなく、キャンセル可能なMB_OKCANCELを使う
    UINT flags = MB_OKCANCEL;
    if (isError) {
        flags |= MB_ICONERROR;
    } else {
        flags |= MB_ICONINFORMATION;
    }
    int result = MessageBoxA(nullptr, message, title, flags);
    return result == IDOK;
}

} // namespace KashipanEngine::Dialogs