#pragma once
#include <vector>
#include <string>
#include "Debug/Logger/LogType.h"

namespace KashipanEngine {

/// @brief 出力ログ設定用構造体
struct LogSettings {
    LogSettings(LogDomain domainType) : domain(domainType) {}
    const LogDomain domain;
};

} // namespace KashipanEngine