#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace KashipanEngine {

/// @brief 文字列からハッシュ値を計算して保持する軽量なタグクラス
/// @details 内部にはハッシュ値のみを保持し、比較（== / !=）はハッシュ値同士で行われるため
///          文字列比較より高速。オブジェクトやコンポーネントの分類・判別用途を想定している。
///          ハッシュはFNV-1a（64bit）で計算され、同じ文字列からは常に同じ値になる。
class Tag final {
public:
    /// @brief 文字列からハッシュ値を計算する（FNV-1a 64bit）
    static constexpr std::uint64_t ComputeHash(const char *str, size_t length) noexcept {
        std::uint64_t hash = 14695981039346656037ull;
        for (size_t i = 0; i < length; ++i) {
            hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(str[i]));
            hash *= 1099511628211ull;
        }
        return hash;
    }
    /// @brief 文字列からハッシュ値を計算する（FNV-1a 64bit）
    static std::uint64_t ComputeHash(const std::string &name) noexcept {
        return ComputeHash(name.c_str(), name.size());
    }

    /// @brief 既定のタグ（空文字列のタグと等しい）
    constexpr Tag() noexcept = default;
    /// @brief 文字列からタグを作成する
    explicit Tag(const std::string &name) noexcept : hash_(ComputeHash(name)) {}

    /// @brief 内部のハッシュ値を取得する
    constexpr std::uint64_t GetHash() const noexcept { return hash_; }
    /// @brief 空文字列のタグ（未設定）かどうか
    constexpr bool IsEmpty() const noexcept { return hash_ == kEmptyHash; }

    constexpr bool operator==(const Tag &other) const noexcept { return hash_ == other.hash_; }
    constexpr bool operator!=(const Tag &other) const noexcept { return hash_ != other.hash_; }

private:
    /// @brief 空文字列のハッシュ値（FNV-1a 64bitのオフセットベース）
    static constexpr std::uint64_t kEmptyHash = 14695981039346656037ull;

    std::uint64_t hash_ = kEmptyHash;
};

} // namespace KashipanEngine
