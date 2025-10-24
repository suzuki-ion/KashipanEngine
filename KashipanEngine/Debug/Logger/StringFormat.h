#pragma once

// 文字列置換ユーティリティ（${Key} を values[Key] で置換）
std::string FormatWithPlaceholders(std::string templ, const std::unordered_map<std::string, std::string>& values) {
    for (const auto &kv : values) {
        const std::string needle = "${" + kv.first + "}";
        size_t pos = 0;
        while ((pos = templ.find(needle, pos)) != std::string::npos) {
            templ.replace(pos, needle.size(), kv.second);
            pos += kv.second.size();
        }
    }
    return templ;
}
