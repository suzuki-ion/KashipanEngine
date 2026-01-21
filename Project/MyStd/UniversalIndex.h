#pragma once
#include <string>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <mutex>
#include <atomic>
#include <type_traits>
#include <utility>

namespace MyStd {

/// @brief 汎用インデックス生成クラス
class UniversalIndex {
public:
    /// @brief 型インデックス取得
    /// @tparam T 型
    /// @return インデックス値
    template <typename T>
    static size_t GetIndex() {
        static size_t index = currentIndex_.fetch_add(1, std::memory_order_relaxed);
        return index;
    }

    /// @brief 文字列インデックス取得
    /// @param str 文字列
    /// @return インデックス値
    static size_t GetIndex(const std::string &str) {
        std::lock_guard<std::mutex> lk(stringMapMutex_);
        auto it = stringIndexMap_.find(str);
        if (it != stringIndexMap_.end()) {
            return it->second;
        } else {
            size_t newIndex = currentIndex_.fetch_add(1, std::memory_order_relaxed);
            stringIndexMap_.emplace(str, newIndex);
            return newIndex;
        }
    }

    /// @brief 汎用引数インデックス取得（例：GetIndex(42, std::string("example"), 3.14f)）
    /// @tparam ...Args 引数型
    /// @param ...args 引数
    /// @return インデックス値
    /// @detail 文字列は const chat * ではなく std::string で渡すこと
    template <typename... Args>
    static size_t GetIndex(const Args&... args) {
        using Key = std::tuple<std::decay_t<Args>...>;
        static std::unordered_map<Key, size_t, TupleHasher<Key>> map;
        static std::mutex mapMutex;

        Key key(std::decay_t<Args>(args)...);

        std::lock_guard<std::mutex> lk(mapMutex);
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        } else {
            size_t newIndex = currentIndex_.fetch_add(1, std::memory_order_relaxed);
            map.emplace(std::move(key), newIndex);
            return newIndex;
        }
    }

private:
    /// @brief 汎用タプルハッシュ（fold式で各要素を combine）
    template <typename Tuple>
    struct TupleHasher {
        size_t operator()(Tuple const& t) const {
            size_t seed = 0;
            std::apply([&seed](auto const&... elems) {
                ((seed ^= std::hash<std::decay_t<decltype(elems)>>{}(elems)
                          + 0x9e3779b97f4a7c15ULL
                          + (seed << 6)
                          + (seed >> 2)), ...);
            }, t);
            return seed;
        }
    };

    static inline std::atomic<size_t> currentIndex_ = 0;
    static inline std::unordered_map<std::string, size_t> stringIndexMap_;
    static inline std::mutex stringMapMutex_;
};

} // namespace MyStd