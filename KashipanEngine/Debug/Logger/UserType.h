#pragma once

// ユーザー定義型の候補かどうか（std:: で始まらず、英字を含む）
bool IsLikelyUserType(std::string_view s) {
    s = StripLeadingQualifiers(StripTrailingRefPtr(s));
    if (s.substr(0, 5) == "std::") return false;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) return true;
    }
    return false;
}
