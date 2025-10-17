#pragma once
#include <vector>
#include <string>
#include <source_location>
#include "Debug/Logger/LogSettings.h"
#include "Debug/Logger/LogType.h"

namespace KashipanEngine::Log {

/// @brief ログ出力
/// @param settings 出力するログの設定
void OutputMessage(const LogSettings &settings);

/// @brief 時間ログ出力
void OutputTime();

/// pbrief 呼び出し元ログ出力
void OutputCaller(const std::source_location location = std::source_location::current());

/// @brief 分離線ログ出力
void OutputSeparator();

/// @brief ログにタブを挿入
void AddTab();

/// @brief ログからタブを削除
void RemoveTab();

} // namespace KashipanEngine::Log