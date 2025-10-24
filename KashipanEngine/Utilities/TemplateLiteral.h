#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <sstream>
#include <ostream>
#include <concepts>

namespace KashipanEngine {

/// @brief 文字列テンプレートの `${name}` プレースホルダーへ値を差し込むユーティリティ
class TemplateLiteral {
public:
    /// @brief 空のテンプレート
    TemplateLiteral() = default;

    /// @brief テンプレート文字列を指定して構築
    explicit TemplateLiteral(std::string_view templ);

    /// @brief テンプレート文字列を取得
    const std::string &Template() const noexcept { return template_; }

    /// @brief テンプレート文字列を設定（プレースホルダーを再解析）
    void SetTemplate(std::string_view templ);

    /// @brief 検出されたプレースホルダー名一覧（重複なし、発見順）
    const std::vector<std::string> &Placeholders() const noexcept { return placeholders_; }

    /// @brief 指定キーの現在値を取得（未設定時は空文字）
    const std::string &Get(std::string_view key) const;

    /// @brief 値の設定（基本型や ostream へ出力可能な型をサポート）
    template <class T>
    void Set(std::string_view key, T &&value) {
        SetImpl(key, std::forward<T>(value));
    }

    /// @brief `${name}` を値へ置換した文字列を生成
    std::string Render() const;

    /// @brief `operator[]` で代入しやすいプロキシ
    class Slot {
    public:
        Slot(TemplateLiteral *owner, std::string key) : owner_(owner), key_(std::move(key)) {}
        template <class T>
        Slot &operator=(T &&v) {
            owner_->Set(key_, std::forward<T>(v));
            return *this;
        }
        // 値参照用に文字列へ変換
        operator const std::string &() const { return owner_->Get(key_); }
    private:
        TemplateLiteral *owner_;
        std::string key_;
    };

    /// @brief キーへの代入用スロットを取得（temp["value"] = 10;）
    Slot operator[](std::string_view key) { return Slot(this, std::string(key)); }

private:
    void ParsePlaceholders();

    // 任意型の文字列化実装（ostream 出力に対応）
    template <class T>
    void SetImpl(std::string_view key, T &&value);

    // 文字列化済みを直接設定
    void SetString(std::string_view key, std::string value);

private:
    std::string template_;
    std::unordered_map<std::string, std::string> values_;
    std::vector<std::string> placeholders_;
};

// ===== Template implementations (must be in header) =====
namespace detail {
    template <class T>
    concept Streamable = requires(std::ostream &os, const T &t) {
        { os << t } -> std::same_as<std::ostream &>;
    };
}

template <class T>
inline void TemplateLiteral::SetImpl(std::string_view key, T &&value) {
    if constexpr (std::is_same_v<std::decay_t<T>, const char *>) {
        SetString(key, std::string(value ? value : ""));
    } else if constexpr (std::is_same_v<std::decay_t<T>, char *>) {
        SetString(key, std::string(value ? value : ""));
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        SetString(key, std::forward<T>(value));
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
        SetString(key, std::string(value));
    } else if constexpr (detail::Streamable<std::decay_t<T>>) {
        std::ostringstream oss;
        oss << value;
        SetString(key, oss.str());
    } else {
        static_assert(sizeof(T) == 0, "TemplateLiteral::Set: value type must be streamable or string-like");
    }
}

} // namespace KashipanEngine
