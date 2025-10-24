#pragma once

// namespace とクラス名を型名から抽出
void ParseNamespaceAndClass(std::string_view typeName, std::string &outNs, std::string &outClass) {
    typeName = StripLeadingQualifiers(StripTrailingRefPtr(typeName));
    // テンプレートの本体名を抽出
    std::string_view cleaned = typeName;
    size_t lt = cleaned.find('<');
    if (lt != std::string_view::npos) cleaned = cleaned.substr(0, lt);
    cleaned = Trim(cleaned);
    if (cleaned.size() >= 2 && cleaned.substr(0, 2) == "::") cleaned.remove_prefix(2);

    size_t last = cleaned.rfind("::");
    if (last != std::string_view::npos) {
        outNs = std::string(Trim(cleaned.substr(0, last)));
        outClass = std::string(Trim(cleaned.substr(last + 2)));
    } else {
        outNs.clear();
        outClass = std::string(cleaned);
    }
}
