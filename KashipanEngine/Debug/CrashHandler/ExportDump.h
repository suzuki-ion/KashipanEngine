#pragma once
#include <Windows.h>
#include <string>

namespace KashipanEngine {
/// @brief ダンプファイルをエクスポートする
/// @param exceptionPointers 例外情報ポインタ（省略時はnullptr）
void ExportDumpFile(EXCEPTION_POINTERS *exceptionPointers = nullptr);
} // namespace KashipanEngine