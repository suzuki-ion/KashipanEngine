#pragma once

// タブ文字列を取得
std::string GetTabString(int depth) {
    const auto &cfg = GetLogSettings();
    if (depth < 0) return "";
    return Repeat(cfg.nestedTabs, depth);
}
