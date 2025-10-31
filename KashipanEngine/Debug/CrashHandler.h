#pragma once
#include <Windows.h>

namespace KashipanEngine {

/// @brief クラッシュハンドラ用関数
/// @param exceptionInfo 例外情報
LONG WINAPI CrashHandler(EXCEPTION_POINTERS *exceptionInfo);

} // namespace KashipanEngine