#pragma once

// ログ用の時間トークンを構築
std::unordered_map<std::string, std::string> BuildTimeTokens() {
    TimeRecord t = GetNowTime();
    auto pad2 = [](int v){ std::ostringstream os; os << std::setw(2) << std::setfill('0') << v; return os.str(); };
    auto pad4 = [](int v){ std::ostringstream os; os << std::setw(4) << std::setfill('0') << v; return os.str(); };

    std::unordered_map<std::string, std::string> tokens;
    tokens["Year"] = pad4(t.year);
    tokens["Month"] = pad2(t.month);
    tokens["Day"] = pad2(t.day);
    tokens["Hour"] = pad2(t.hour);
    tokens["Minute"] = pad2(t.minute);
    tokens["Second"] = pad2(static_cast<int>(t.second));
    tokens["BuildType"] = kBuildTypeString;
    return tokens;
}
