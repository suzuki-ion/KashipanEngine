#include "SourceLocation.h"
#include <cctype>

namespace KashipanEngine {
namespace {

std::string Trim(std::string_view sv) {
    size_t b = 0, e = sv.size();
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

void CollapseSpaces(std::string &s) {
    std::string out;
    out.reserve(s.size());
    bool inSpace = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!inSpace) {
                out.push_back(' ');
                inSpace = true;
            }
        } else {
            out.push_back(c);
            inSpace = false;
        }
    }
    s = Trim(out);
}

void ReplaceAll(std::string &s, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

void EraseToken(std::string &s, std::string_view token) {
    ReplaceAll(s, token, "");
}

std::vector<std::string> SplitByCommaKeepingTemplates(std::string_view sv) {
    std::vector<std::string> result;
    int angle = 0;
    int paren = 0;
    int brace = 0;
    size_t start = 0;
    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '<') ++angle;
        else if (c == '>') { if (angle > 0) --angle; }
        else if (c == '(') ++paren;
        else if (c == ')') { if (paren > 0) --paren; }
        else if (c == '{') ++brace;
        else if (c == '}') { if (brace > 0) --brace; }
        else if (c == ',' && angle == 0 && paren == 0 && brace == 0) {
            result.emplace_back(Trim(sv.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start <= sv.size()) {
        result.emplace_back(Trim(sv.substr(start)));
    }
    if (result.size() == 1) {
        auto t = Trim(result[0]);
        if (t.empty() || t == "void") result.clear();
    }
    return result;
}

std::vector<std::string> SplitScopeKeepingTemplates(std::string_view sv) {
    std::vector<std::string> parts;
    int angle = 0;
    size_t tokenBegin = 0;
    for (size_t i = 0; i + 1 < sv.size(); ++i) {
        char c = sv[i];
        if (c == '<') ++angle;
        else if (c == '>') { if (angle > 0) --angle; }
        if (angle == 0 && sv[i] == ':' && sv[i + 1] == ':') {
            parts.emplace_back(Trim(sv.substr(tokenBegin, i - tokenBegin)));
            i += 1;
            tokenBegin = i + 1;
        }
    }
    if (tokenBegin < sv.size()) {
        parts.emplace_back(Trim(sv.substr(tokenBegin)));
    } else if (tokenBegin == sv.size()) {
        parts.emplace_back(std::string());
    }

    std::vector<std::string> filtered;
    filtered.reserve(parts.size());
    for (auto &p : parts) if (!p.empty()) filtered.push_back(p);
    return filtered;
}

bool LooksLikeClassToken(std::string_view tok) {
    if (tok.find('<') != std::string::npos) return true;
    if (!tok.empty() && std::isalpha(static_cast<unsigned char>(tok.front()))) {
        if (std::isupper(static_cast<unsigned char>(tok.front()))) return true;
    }
    if (tok.find("_t") != std::string::npos) return true;
    return false;
}

} // namespace

FunctionSignatureInfo ParseFunctionSignature(std::string sig) {
    FunctionSignatureInfo out{};
    out.rawSignature = sig;

    if (!sig.empty()) {
        size_t rparen = sig.rfind(')');
        if (rparen != std::string::npos && rparen + 1 < sig.size()) {
            out.trailingQualifiers = Trim(std::string_view(sig).substr(rparen + 1));
        }
    }

    // 呼出規約などを除去
    EraseToken(sig, "__cdecl");
    EraseToken(sig, "__thiscall");
    EraseToken(sig, "__stdcall");
    EraseToken(sig, "__fastcall");
    EraseToken(sig, "__vectorcall");

    CollapseSpaces(sig);

    size_t lparen = sig.find('(');
    size_t rparen2 = sig.rfind(')');
    if (lparen == std::string::npos || rparen2 == std::string::npos || rparen2 < lparen) {
        out.functionName = sig;
        return out;
    }

    std::string pre = Trim(std::string_view(sig).substr(0, lparen));
    std::string args = Trim(std::string_view(sig).substr(lparen + 1, rparen2 - lparen - 1));

    out.arguments = SplitByCommaKeepingTemplates(args);

    // 戻り値型を取り出すために先頭の型キーワードを落とす
    auto StripTypeKeyPrefixes = [](std::string &s) {
        ReplaceAll(s, "class ", "");
        ReplaceAll(s, "struct ", "");
        ReplaceAll(s, "enum ", "");
        ReplaceAll(s, "union ", "");
    };

    StripTypeKeyPrefixes(pre);

    size_t lastScope = pre.rfind("::");
    size_t funcStart = std::string::npos;
    if (lastScope != std::string::npos) {
        funcStart = lastScope + 2;
    } else {
        size_t lastSpace = pre.rfind(' ');
        funcStart = (lastSpace == std::string::npos) ? 0 : lastSpace + 1;
    }

    std::string funcToken = pre.substr(funcStart);
    out.functionName = Trim(funcToken);

    // 戻り値型
    if (lastScope != std::string::npos) {
        size_t lastSpaceBeforeScope = pre.rfind(' ', lastScope);
        if (lastSpaceBeforeScope == std::string::npos) {
            out.returnType.clear();
        } else {
            out.returnType = Trim(std::string_view(pre).substr(0, lastSpaceBeforeScope));
            StripTypeKeyPrefixes(out.returnType);
        }
    } else {
        size_t lastSpace = pre.rfind(' ');
        if (lastSpace != std::string::npos) {
            out.returnType = Trim(std::string_view(pre).substr(0, lastSpace));
            StripTypeKeyPrefixes(out.returnType);
        } else {
            out.returnType.clear();
        }
    }

    // スコープ部分（namespace / class を区別せずにそのまま格納）
    std::string scopePart;
    if (lastScope != std::string::npos) {
        size_t lastSpaceBeforeScope = pre.rfind(' ', lastScope);
        size_t scopeBegin = (lastSpaceBeforeScope == std::string::npos) ? 0 : lastSpaceBeforeScope + 1;
        scopePart = Trim(std::string_view(pre).substr(scopeBegin, lastScope - scopeBegin));
        StripTypeKeyPrefixes(scopePart);
    }

    out.scopes = SplitScopeKeepingTemplates(scopePart);

    // コンストラクタ/デストラクタっぽいものは戻り値型を空にする（慣習的に）
    if (!out.scopes.empty()) {
        const std::string &lastScopeToken = out.scopes.back();
        bool isCtor = (out.functionName == lastScopeToken);
        bool isDtor = (!out.functionName.empty() && out.functionName[0] == '~' && out.functionName.substr(1) == lastScopeToken);
        if (isCtor || isDtor) {
            out.returnType.clear();
        }
    }

    return out;
}

FunctionSignatureInfo ParseFunctionSignature(const std::source_location &loc) {
    return ParseFunctionSignature(std::string(loc.function_name()));
}

SourceLocationInfo MakeSourceLocationInfo(const std::source_location &loc) {
    SourceLocationInfo info{};
    info.filePath = loc.file_name();
    info.line = loc.line();
    info.column = loc.column();
    info.signature = ParseFunctionSignature(loc);
    info.raw = loc;
    return info;
}

} // namespace KashipanEngine
