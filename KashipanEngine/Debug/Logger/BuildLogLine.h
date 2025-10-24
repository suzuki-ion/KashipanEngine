#pragma once

// ログ行を構築
std::string BuildLogLine(const ScopeFrame* frame, LogSeverity severity, const std::string& message) {
    const auto &cfg = GetLogSettings();

    // TemplateLiteral を使ってプレースホルダ置換
    TemplateLiteral tpl(cfg.logMessageFormat);

    // 時刻系トークンを投入
    {
        auto timeTokens = BuildTimeTokens();
        for (const auto &kv : timeTokens) {
            tpl.Set(kv.first, kv.second);
        }
    }

    // 重大度・メッセージ
    tpl.Set("Severity", kLogSeverityLabel.at(severity));
    tpl.Set("Message", message);

    // インデントとスコープ情報
    const int depth = frame ? frame->depth : sTabCount;
    tpl.Set("NestedTabs", GetTabString(depth));
    const ScopeFrame* top = frame ? frame : GetTopScopeFrame();
    tpl.Set("namespace", top ? top->namespaceName : std::string());
    tpl.Set("class",     top ? top->className     : std::string());
    tpl.Set("function",  top ? top->functionName  : std::string());

    std::string line = tpl.Render();
    line += '\n';
    return line;
}
