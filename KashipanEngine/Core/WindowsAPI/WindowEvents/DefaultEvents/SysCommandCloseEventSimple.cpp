#include "SysCommandCloseEventSimple.h"
#include <Windows.h>
#include "Core/Window.h"
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SysCommandCloseEventSimple::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    GetWindow()->Destroy();
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
