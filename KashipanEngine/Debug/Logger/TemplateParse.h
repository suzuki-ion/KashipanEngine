#pragma once

// angle brackets の対応を取りながら、先頭のテンプレート引数列を抽出
bool ExtractFirstTemplateArgList(std::string_view s, std::string_view &outArgs) {
    size_t lt = s.find('<');
    if (lt == std::string_view::npos) return false;
    int depth = 0;
    for (size_t i = lt; i < s.size(); ++i) {
        char c = s[i];
        if (c == '<') ++depth;
        else if (c == '>') {
            --depth;
            if (depth == 0) {
                outArgs = s.substr(lt + 1, i - (lt + 1));
                return true;
            }
        }
    }
    return false;
}

// トップレベルのカンマでテンプレート引数を分割
std::vector<std::string_view> SplitTemplateArgs(std::string_view args) {
    std::vector<std::string_view> result;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        char c = args[i];
        if (c == '<') ++depth;
        else if (c == '>') --depth;
        else if (c == ',' && depth == 0) {
            result.emplace_back(Trim(args.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < args.size()) result.emplace_back(Trim(args.substr(start)));
    return result;
}
