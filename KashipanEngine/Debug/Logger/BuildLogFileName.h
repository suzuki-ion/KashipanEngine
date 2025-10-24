#pragma once

// ログファイル名を構築
std::string BuildLogFileName() {
    const auto &cfg = GetLogSettings();
    TemplateLiteral tpl(cfg.logFileFormat);

    auto tokens = BuildTimeTokens();
    for (const auto &kv : tokens) tpl.Set(kv.first, kv.second);

    std::string name = tpl.Render();
    if (name.size() < 4 || name.substr(name.size() - 4) != ".log") {
        name += ".log";
    }
    return name;
}
