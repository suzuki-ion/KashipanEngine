#include "MessageDialog.h"
#include <Windows.h>

namespace KashipanEngine::Dialogs {

bool ShowMessageDialog(const char *title, const char *message, bool isError) {
    UINT flags = MB_OK;
    if (isError) {
        flags |= MB_ICONERROR;
    } else {
        flags |= MB_ICONINFORMATION;
    }
    int result = MessageBoxA(nullptr, message, title, flags);
    return result == IDOK;
}

} // namespace KashipanEngine::Dialogs