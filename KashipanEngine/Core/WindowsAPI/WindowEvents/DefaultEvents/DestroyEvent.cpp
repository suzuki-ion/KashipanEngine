#include "DestroyEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> DestroyEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    PostQuitMessage(0);
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
