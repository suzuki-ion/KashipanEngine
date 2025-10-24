#pragma once

// 指定文字列を n 回連結
std::string Repeat(const std::string &unit, int n) {
    if (n <= 0 || unit.empty()) return "";
    std::string out;
    out.reserve(unit.size() * static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) out += unit;
    return out;
}
