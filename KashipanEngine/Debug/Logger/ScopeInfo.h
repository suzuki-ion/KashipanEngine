#pragma once

// 情報保持用のスコープ情報構造体
struct ScopeInfo {
    // 自作部分の情報
    std::string namespaceName;  // 例: KashipanEngine
    std::string className;      // 例: GameEngine
    std::string functionName;   // 例: Initialize / operator() など（自作スコープに属する場合）

    // 外部部分の情報
    std::string externalNamespaceName;  // 例: std
    std::string externalFunctionName;   // 例: make_unique
    std::string rawSignature;           // source_location::function_name の生文字列
};

// source_location からスコープ情報を抽出（自作部分のみログに使用。std 部分は保持のみ）
ScopeInfo GetScopeInfo(const std::source_location &location) {
    ScopeInfo info{};
    std::string sig = location.function_name();
    info.rawSignature = sig;

    // パラメータ以降を削除
    size_t lparen = sig.find('(');
    if (lparen != std::string::npos) sig = sig.substr(0, lparen);

    // MSVC の呼出規約トークン以降を使用
    size_t convPos = sig.find("__cdecl");
    if (convPos != std::string::npos) {
        sig = sig.substr(convPos + 7); // "__cdecl" の長さ
    } else {
        // フォールバック: 直近の空白以降を使用（戻り値型を除去）
        size_t lastSpace = sig.rfind(' ');
        if (lastSpace != std::string::npos) {
            sig = sig.substr(lastSpace + 1);
        }
    }

    // トリムと先頭の :: 除去
    std::string_view view = Trim(sig);
    if (view.size() >= 2 && view.substr(0, 2) == "::") view.remove_prefix(2);

    // まず先頭トークン（例: std::make_unique）を確認
    std::string_view argsView;
    bool hasTemplateArgs = ExtractFirstTemplateArgList(view, argsView);

    // 先頭スコープ（< の手前）
    std::string_view head = view;
    size_t headEnd = head.find('<');
    if (headEnd != std::string_view::npos) head = head.substr(0, headEnd);
    head = Trim(head);

    // "std::..." を検出
    bool headIsStd = head.size() >= 5 && head.substr(0, 5) == "std::";
    if (headIsStd) {
        // 保持のみ（ログには出さない）
        info.externalNamespaceName = "std";
        size_t fnSep = head.rfind("::");
        if (fnSep != std::string::npos) {
            info.externalFunctionName = std::string(head.substr(fnSep + 2));
        } else {
            info.externalFunctionName = std::string(head);
        }

        // テンプレート引数からユーザー型を抽出
        if (hasTemplateArgs) {
            auto tokens = SplitTemplateArgs(argsView);
            for (auto t : tokens) {
                t = StripLeadingQualifiers(StripTrailingRefPtr(t));
                if (!IsLikelyUserType(t)) continue;
                if (t.substr(0, 5) == "std::") continue;
                ParseNamespaceAndClass(t, info.namespaceName, info.className);
                break;
            }
        }

        return info;
    }

    // 通常ケース: 自作の関数/メンバ。スコープ区切りで分割
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= view.size()) {
        size_t pos = view.find("::", start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(view.substr(start));
            break;
        } else {
            parts.emplace_back(view.substr(start, pos - start));
            start = pos + 2;
        }
    }

    if (!parts.empty()) {
        // 関数名（テンプレート名を除去）
        std::string_view funcToken = Trim(parts.back());
        size_t lt2 = funcToken.find('<');
        if (lt2 != std::string_view::npos) funcToken = funcToken.substr(0, lt2);
        info.functionName = std::string(funcToken);

        if (parts.size() >= 2) {
            info.className = std::string(Trim(parts[parts.size() - 2]));
            if (info.className.rfind("std", 0) == 0) {
                info.className.clear();
            }
        }
        if (parts.size() >= 3) {
            std::string ns;
            for (size_t i = 0; i + 2 < parts.size(); ++i) {
                std::string token = std::string(Trim(parts[i]));
                if (token.rfind("std", 0) == 0) continue;
                if (!ns.empty()) ns += "::";
                ns += token;
            }
            info.namespaceName = std::move(ns);
        }
    }

    return info;
}
