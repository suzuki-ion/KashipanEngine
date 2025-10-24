#pragma once

// 文字列の前後空白を除去
std::string_view Trim(std::string_view s) {
    const auto is_space = [](unsigned char c){ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t start = 0;
    while (start < s.size() && is_space(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && is_space(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}
