#pragma once

// 先頭の修飾子やキーワードを除去
std::string_view StripLeadingQualifiers(std::string_view s) {
    s = Trim(s);
    auto starts_with = [](std::string_view a, std::string_view b){ return a.substr(0, b.size()) == b; };
    bool changed = true;
    while (changed) {
        changed = false;
        if (starts_with(s, "const ")) { s.remove_prefix(6); changed = true; }
        if (starts_with(s, "volatile ")) { s.remove_prefix(9); changed = true; }
        if (starts_with(s, "class ")) { s.remove_prefix(6); changed = true; }
        if (starts_with(s, "struct ")) { s.remove_prefix(7); changed = true; }
        if (starts_with(s, "enum ")) { s.remove_prefix(5); changed = true; }
        if (starts_with(s, "union ")) { s.remove_prefix(6); changed = true; }
    }
    return Trim(s);
}

// 後方の参照・ポインタ修飾を取り除く
std::string_view StripTrailingRefPtr(std::string_view s) {
    s = Trim(s);
    while (!s.empty()) {
        char back = s.back();
        if (back == '&' || back == '*') {
            s.remove_suffix(1);
            s = Trim(s);
        } else {
            break;
        }
    }
    return s;
}
