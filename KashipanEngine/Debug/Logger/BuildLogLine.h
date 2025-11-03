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

    // scopes はテンプレート側の join 機能で連結するため、そのままベクタで渡す
    std::vector<std::string> outScopes;
    if (top) {
        const auto &sc = top->scopes;
        // ログ設定で namespace/class 出力がどちらも有効な場合はそのまま渡す
        if (cfg.enableOutputNamespace && cfg.enableOutputClass) {
            outScopes = sc;
        } else if (cfg.enableOutputNamespace) {
            // namespace 出力のみ有効な場合はログ設定の namespaces に含まれるものを渡す
            for (const auto &s : sc) {
                if (std::find(cfg.namespaces.begin(), cfg.namespaces.end(), s) != cfg.namespaces.end()) {
                    outScopes.push_back(s);
                }
            }
        } else if (cfg.enableOutputClass) {
            // class 出力のみ有効な場合はログ設定の namespaces に含まれないものを渡す
            for (const auto &s : sc) {
                if (std::find(cfg.namespaces.begin(), cfg.namespaces.end(), s) == cfg.namespaces.end()) {
                    outScopes.push_back(s);
                }
            }
        }

    }
    tpl.Set("scopes", outScopes);

    tpl.Set("function",  top ? top->functionName  : std::string());

    // 旧プレースホルダ互換（空、またはスコープ末尾を class として提供）
    if (top && !top->scopes.empty()) {
        tpl.Set("class", top->scopes.back());
        // namespace は曖昧なので、先頭〜末尾-1 を連結して提供
        std::string ns;
        if (!outScopes.empty()) {
            // outScopes から namespace 相当を再構築（末尾を class とみなす）
            if (outScopes.size() > 1) {
                for (size_t i = 0; i + 1 < outScopes.size(); ++i) {
                    if (i) ns += "::";
                    ns += outScopes[i];
                }
            }
        }
        tpl.Set("namespace", ns);
    } else {
        tpl.Set("class", std::string());
        tpl.Set("namespace", std::string());
    }

    std::string line = tpl.Render();
    line += '\n';
    return line;
}
