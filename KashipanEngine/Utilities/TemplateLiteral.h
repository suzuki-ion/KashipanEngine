#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <sstream>
#include <ostream>
#include <concepts>
#include <iterator>

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

    /// @brief 指定のプレースホルダーがテンプレート内に存在するか
    bool HasPlaceholder(std::string_view key) const noexcept;

    /// @brief 値の設定（基本型や ostream へ出力可能な型、コンテナ型をサポート）
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
    // スカラー（単一値）の文字列化表現
    std::unordered_map<std::string, std::string> values_;
    // コンテナの各要素の文字列一覧（join などで使用）
    std::unordered_map<std::string, std::vector<std::string>> listValues_;
    std::vector<std::string> placeholders_;
};

// ===== Template implementations (must be in header) =====
namespace detail {
    template <class T>
    concept Streamable = requires(std::ostream &os, const T &t) {
        { os << t } -> std::same_as<std::ostream &>;
    };

    template <class T>
    concept StringLike = std::is_same_v<std::decay_t<T>, std::string>
        || std::is_same_v<std::decay_t<T>, std::string_view>
        || std::is_same_v<std::decay_t<T>, const char*>
        || std::is_same_v<std::decay_t<T>, char*>;

    template <class T>
    concept Iterable = requires(T t) {
        { std::begin(t) };
        { std::end(t) };
    };

    template <class T>
    concept ContainerLike = Iterable<T> && (!StringLike<T>);

    inline std::string ToString(std::string s) { return s; }
    inline std::string ToString(std::string_view sv) { return std::string(sv); }
    inline std::string ToString(const char* s) { return std::string(s ? s : ""); }
    inline std::string ToString(char* s) { return std::string(s ? s : ""); }
    template <class T>
    requires Streamable<std::decay_t<T>>
    inline std::string ToString(const T& v) {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }
}

template <class T>
inline void TemplateLiteral::SetImpl(std::string_view key, T &&value) {
    using DT = std::decay_t<T>;
    if constexpr (detail::ContainerLike<DT>) {
        // コンテナ要素を文字列化して保持
        std::vector<std::string> items;
        // 事前にサイズが取れる場合のみ予約
        if constexpr (requires(const DT& c){ c.size(); }) {
            items.reserve(value.size());
        }
        for (const auto &elem : value) {
            // 要素は文字列ライク or ストリーム出力可能を想定
            items.emplace_back(detail::ToString(elem));
        }
        listValues_[std::string(key)] = items;
        // `${name}` でもそこそこ見やすく出すため、デフォルトは ", " で結合
        std::ostringstream oss;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i) oss << ", ";
            oss << items[i];
        }
        SetString(key, oss.str());
    } else if constexpr (std::is_same_v<DT, const char *>) {
        SetString(key, std::string(value ? value : ""));
    } else if constexpr (std::is_same_v<DT, char *>) {
        SetString(key, std::string(value ? value : ""));
    } else if constexpr (std::is_same_v<DT, std::string>) {
        SetString(key, std::forward<T>(value));
    } else if constexpr (std::is_same_v<DT, std::string_view>) {
        SetString(key, std::string(value));
    } else if constexpr (detail::Streamable<DT>) {
        std::ostringstream oss;
        oss << value;
        SetString(key, oss.str());
    } else {
        static_assert(sizeof(T) == 0, "TemplateLiteral::Set: value type must be streamable/string-like or a supported container");
    }
}

} // namespace KashipanEngine
