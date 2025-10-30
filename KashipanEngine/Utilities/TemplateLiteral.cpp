#include "TemplateLiteral.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <concepts>

namespace KashipanEngine {

namespace {
// `${...}` の範囲検出（簡易・最初の '}' まで）
std::vector<std::pair<size_t, size_t>> FindTokenRanges(const std::string &s) {
    std::vector<std::pair<size_t, size_t>> ranges; // [begin, end) of ${...}
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        if (s[i] == '$' && s[i + 1] == '{') {
            size_t j = i + 2;
            while (j < s.size() && s[j] != '}') {
                ++j;
            }
            if (j < s.size() && s[j] == '}') {
                ranges.emplace_back(i, j + 1);
                i = j; // 次探索位置
            }
        }
    }
    return ranges;
}

enum class TokenKind { Placeholder, IfOpen, IfClose, Unknown };

struct TokenInfo {
    TokenKind kind = TokenKind::Unknown;
    std::string key;     // 置換キー（Placeholder）
    std::string prefix;  // 値の前に付ける（Placeholder）
    std::string suffix;  // 値の後に付ける（Placeholder）
    bool hasFormat = false; // `?` が含まれていたか（Placeholder）
    bool negate = false;    // 旧: IfOpen 用（互換維持・未使用）
    std::string condExpr;   // IfOpen: 条件式全文（例: "value1 || !value2 && value3"）
};

// `${key?pre:suf}` / `${#if ...}` / `${/if}` を解析
TokenInfo ParseToken(std::string_view token) {
    TokenInfo info;
    // token: ${...}
    if (token.size() >= 3 && token[0] == '$' && token[1] == '{' && token.back() == '}') {
        token.remove_prefix(2);
        token.remove_suffix(1);
        // 前後空白をトリム
        auto l = token.find_first_not_of(" \t\n\r");
        auto r = token.find_last_not_of(" \t\n\r");
        if (l == std::string_view::npos) return info;
        std::string_view core = token.substr(l, r - l + 1);

        // if-close
        if (core.size() >= 3 && core[0] == '/' && core.substr(1) == "if") {
            info.kind = TokenKind::IfClose;
            return info;
        }
        // if-open: "#if <expr>"
        if (core.size() >= 3 && core[0] == '#' && core.substr(1, 2) == "if") {
            // 残りを式として取得
            std::string_view rest = core.substr(3); // after "#if"
            // トリム
            auto kl = rest.find_first_not_of(" \t\n\r");
            auto kr = rest.find_last_not_of(" \t\n\r");
            if (kl != std::string_view::npos) rest = rest.substr(kl, kr - kl + 1);
            info.kind = TokenKind::IfOpen;
            info.condExpr.assign(rest.begin(), rest.end());
            return info;
        }

        // 通常のプレースホルダー
        size_t qpos = core.find('?');
        if (qpos == std::string_view::npos) {
            // フォーマットなし
            info.kind = TokenKind::Placeholder;
            info.key.assign(core.begin(), core.end());
            return info;
        }

        std::string_view key = core.substr(0, qpos);
        // キーの前後空白をトリム
        auto kl2 = key.find_first_not_of(" \t\n\r");
        auto kr2 = key.find_last_not_of(" \t\n\r");
        if (kl2 != std::string_view::npos) key = key.substr(kl2, kr2 - kl2 + 1);
        info.key.assign(key.begin(), key.end());

        std::string_view fmt = core.substr(qpos + 1);
        size_t cpos = fmt.find(':');
        if (cpos == std::string_view::npos) {
            info.prefix.assign(fmt.begin(), fmt.end());
            info.hasFormat = true;
        } else {
            std::string_view pre = fmt.substr(0, cpos);
            std::string_view suf = fmt.substr(cpos + 1);
            info.prefix.assign(pre.begin(), pre.end());
            info.suffix.assign(suf.begin(), suf.end());
            info.hasFormat = true;
        }
        info.kind = TokenKind::Placeholder;
    }
    return info;
}

// `${#if ...}` に対する対応する `${/if}` を探す（ネスト対応）
// 見つかった場合は pair(closeBegin, closeEnd) を返す。見つからなければ {npos, npos}。
std::pair<size_t, size_t> FindMatchingIfClose(const std::string &s, size_t from) {
    size_t i = from;
    int depth = 0;
    while (i < s.size()) {
        size_t open = s.find("${", i);
        if (open == std::string::npos) break;
        size_t closeBrace = s.find('}', open + 2);
        if (closeBrace == std::string::npos) break;
        std::string_view tv(&s[open], closeBrace - open + 1);
        TokenInfo t = ParseToken(tv);
        if (t.kind == TokenKind::IfOpen) {
            ++depth;
        } else if (t.kind == TokenKind::IfClose) {
            if (depth == 0) {
                return { open, closeBrace + 1 };
            }
            --depth;
        }
        i = closeBrace + 1;
    }
    return { std::string::npos, std::string::npos };
}

// 論理式パーサ（!、&&、|| と識別子をサポート）
struct BoolExprParser {
    std::string_view s;
    size_t pos = 0;
    const std::unordered_map<std::string, std::string> &values;
    bool ok = true;

    BoolExprParser(std::string_view sv, const std::unordered_map<std::string, std::string> &vals)
        : s(sv), values(vals) {}

    void SkipWs() {
        while (pos < s.size()) {
            char c = s[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos; else break;
        }
    }
    bool StartsWith(std::string_view lit) const {
        return s.substr(pos, lit.size()) == lit;
    }
    bool IsOpChar(char c) const { return c == '!' || c == '&' || c == '|' || c == '(' || c == ')'; }

    bool Parse(bool &out) {
        SkipWs();
        bool v = false;
        if (!ParseOr(v)) { ok = false; out = false; return false; }
        SkipWs();
        if (pos != s.size()) { ok = false; out = false; return false; }
        out = v;
        return true;
    }

    bool ParseOr(bool &out) {
        if (!ParseAnd(out)) return false;
        while (true) {
            SkipWs();
            if (StartsWith("||")) {
                pos += 2;
                bool rhs = false;
                if (!ParseAnd(rhs)) return false;
                out = out || rhs;
            } else {
                break;
            }
        }
        return true;
    }
    bool ParseAnd(bool &out) {
        if (!ParseUnary(out)) return false;
        while (true) {
            SkipWs();
            if (StartsWith("&&")) {
                pos += 2;
                bool rhs = false;
                if (!ParseUnary(rhs)) return false;
                out = out && rhs;
            } else {
                break;
            }
        }
        return true;
    }
    bool ParseUnary(bool &out) {
        SkipWs();
        if (pos < s.size() && s[pos] == '!') {
            ++pos;
            bool v = false;
            if (!ParseUnary(v)) return false;
            out = !v;
            return true;
        }
        return ParsePrimary(out);
    }
    bool ParsePrimary(bool &out) {
        SkipWs();
        if (pos < s.size() && s[pos] == '(') {
            ++pos;
            bool v = false;
            if (!ParseOr(v)) return false;
            SkipWs();
            if (pos >= s.size() || s[pos] != ')') return false;
            ++pos;
            out = v;
            return true;
        }
        // 識別子（演算子と空白以外の連続）
        size_t start = pos;
        while (pos < s.size()) {
            char c = s[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || IsOpChar(c)) break;
            ++pos;
        }
        if (start == pos) return false; // 何も読めない
        std::string key(s.substr(start, pos - start));
        auto it = values.find(key);
        out = (it != values.end() && !it->second.empty());
        return true;
    }
};

static bool EvaluateCondition(std::string_view expr,
    const std::unordered_map<std::string, std::string> &values) {
    BoolExprParser p(expr, values);
    bool result = false;
    if (!p.Parse(result)) return false; // パース失敗は偽扱い
    return result;
}

} // namespace

TemplateLiteral::TemplateLiteral(std::string_view templ) {
    SetTemplate(templ);
}

void TemplateLiteral::SetTemplate(std::string_view templ) {
    template_.assign(templ.begin(), templ.end());
    ParsePlaceholders();
}

const std::string &TemplateLiteral::Get(std::string_view key) const {
    static const std::string kEmpty;
    auto it = values_.find(std::string(key));
    if (it == values_.end()) return kEmpty;
    return it->second;
}

bool TemplateLiteral::HasPlaceholder(std::string_view key) const noexcept {
    // placeholders_ は重複無しの発見順。線形検索で十分。
    for (const auto &k : placeholders_) {
        if (k == key) return true;
    }
    return false;
}

void TemplateLiteral::SetString(std::string_view key, std::string value) {
    values_[std::string(key)] = std::move(value);
}

void TemplateLiteral::ParsePlaceholders() {
    placeholders_.clear();
    std::unordered_set<std::string> seen;
    auto ranges = FindTokenRanges(template_);
    for (auto [b, e] : ranges) {
        TokenInfo t = ParseToken(std::string_view(template_).substr(b, e - b));
        if (t.kind == TokenKind::Placeholder && !t.key.empty() && !seen.count(t.key)) {
            placeholders_.push_back(t.key);
            seen.insert(t.key);
        }
    }
}

// 範囲 [start, end) をレンダリング（if ブロック対応、ネスト可）
static void RenderRange(const std::string &templ,
                        const std::unordered_map<std::string, std::string> &values,
                        size_t start, size_t end,
                        std::string &out) {
    size_t i = start;
    while (i < end) {
        size_t tokBegin = templ.find("${", i);
        if (tokBegin == std::string::npos || tokBegin >= end) {
            // 残りは固定文字列
            out.append(templ.data() + i, end - i);
            return;
        }
        // 固定部
        if (tokBegin > i) out.append(templ.data() + i, tokBegin - i);

        // トークン終端
        size_t tokEnd = templ.find('}', tokBegin + 2);
        if (tokEnd == std::string::npos || tokEnd >= end) {
            // '}' が無い、または範囲外 → そのまま出力して終了
            out.append(templ.data() + tokBegin, end - tokBegin);
            return;
        }

        std::string_view tokenView(templ.data() + tokBegin, tokEnd - tokBegin + 1);
        TokenInfo t = ParseToken(tokenView);
        switch (t.kind) {
        case TokenKind::Placeholder: {
            auto it = values.find(t.key);
            if (it != values.end()) {
                if (t.hasFormat) {
                    out.append(t.prefix);
                    out.append(it->second);
                    out.append(t.suffix);
                } else {
                    out.append(it->second);
                }
            } else {
                // 未設定は原文のまま
                out.append(tokenView.begin(), tokenView.end());
            }
            i = tokEnd + 1;
            break;
        }
        case TokenKind::IfOpen: {
            // 対応する /if を探す（ネスト対応）
            auto [ifCloseBegin, ifCloseEnd] = FindMatchingIfClose(templ, tokEnd + 1);
            if (ifCloseBegin == std::string::npos) {
                // 閉じが無ければリテラルとして出力
                out.append(tokenView.begin(), tokenView.end());
                i = tokEnd + 1;
                break;
            }
            // 条件評価: 論理式を評価（存在かつ非空を真）
            bool cond = EvaluateCondition(t.condExpr, values);
            if (cond) {
                // ブロック内容を再帰レンダリング
                RenderRange(templ, values, tokEnd + 1, ifCloseBegin, out);
            }
            // ブロックの終わりの直後へ
            i = ifCloseEnd;
            break;
        }
        case TokenKind::IfClose: {
            // 単独の /if は不正。リテラルとして出力して進める
            out.append(tokenView.begin(), tokenView.end());
            i = tokEnd + 1;
            break;
        }
        default: {
            // 未知トークンはそのまま
            out.append(tokenView.begin(), tokenView.end());
            i = tokEnd + 1;
            break;
        }
        }
    }
}

std::string TemplateLiteral::Render() const {
    if (template_.empty()) return {};
    std::string out;
    out.reserve(template_.size() + 32);
    RenderRange(template_, values_, 0, template_.size(), out);
    return out;
}

} // namespace KashipanEngine
