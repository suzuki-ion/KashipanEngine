#pragma once
#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace KashipanEngine {

/// @brief 固定長チャンクを連ねて要素を格納するプール
/// @details 要素は一度配置されると絶対に再配置されない（チャンク自体・チャンク内スロットのアドレスは
///          プールが破棄されるまで不変）。外側の管理配列が伸びてもチャンクへのポインタが移動するだけで、
///          チャンクの中身やチャンク自体のアドレスには影響しない。
/// @tparam T 格納する要素の型
/// @tparam kChunkSize 1チャンクあたりの要素数
template <typename T, size_t kChunkSize = 256>
class ChunkedPool {
public:
    ChunkedPool() = default;
    ~ChunkedPool() = default;

    ChunkedPool(const ChunkedPool &) = delete;
    ChunkedPool &operator=(const ChunkedPool &) = delete;
    ChunkedPool(ChunkedPool &&) = delete;
    ChunkedPool &operator=(ChunkedPool &&) = delete;

    /// @brief デフォルト構築で要素を追加
    /// @return 追加された要素へのポインタ
    T *EmplaceDefault() { return Emplace(); }

    /// @brief 引数を転送し、最終的な格納場所へ直接配置構築する（ムーブ・コピーは発生しない）
    /// @param args コンストラクタへ転送する引数
    /// @return 追加された要素へのポインタ
    template <typename... Args>
    T *Emplace(Args &&...args) {
        size_t index;
        if (!freeIndices_.empty()) {
            index = freeIndices_.back();
            freeIndices_.pop_back();
        } else {
            index = size_;
            ++size_;
            EnsureChunk(index);
        }
        Slot &slot = SlotAt(index);
        slot.emplace(std::forward<Args>(args)...);
        T *ptr = &*slot;
        indexByPointer_[ptr] = index;
        return ptr;
    }

    /// @brief 要素を破棄し、スロットを再利用可能にする（要素自体のアドレスは他のスロットに影響しない）
    /// @param ptr 破棄する要素へのポインタ
    /// @return 破棄に成功した場合はtrue
    bool Remove(const T *ptr) {
        auto it = indexByPointer_.find(ptr);
        if (it == indexByPointer_.end()) return false;
        size_t index = it->second;
        indexByPointer_.erase(it);
        SlotAt(index).reset();
        freeIndices_.push_back(index);
        return true;
    }

    /// @brief このプールが指定ポインタの要素を現在所有しているか
    bool Owns(const T *ptr) const { return indexByPointer_.contains(ptr); }

    /// @brief 現在生存している要素数
    size_t LiveCount() const { return indexByPointer_.size(); }

    /// @brief 全要素を破棄し、確保済みチャンクも含めて完全にリセットする
    void Clear() {
        chunks_.clear();
        freeIndices_.clear();
        indexByPointer_.clear();
        size_ = 0;
    }

private:
    using Slot = std::optional<T>;
    using Chunk = std::array<Slot, kChunkSize>;

    void EnsureChunk(size_t index) {
        size_t chunkIndex = index / kChunkSize;
        if (chunkIndex >= chunks_.size()) {
            chunks_.push_back(std::make_unique<Chunk>());
        }
    }

    Slot &SlotAt(size_t index) {
        return (*chunks_[index / kChunkSize])[index % kChunkSize];
    }

    /// @brief 確保済みチャンクへのポインタの配列（このvector自体が伸びてもチャンクの中身は動かない）
    std::vector<std::unique_ptr<Chunk>> chunks_;
    /// @brief 削除により空いたスロットのグローバルインデックス（LIFOで再利用）
    std::vector<size_t> freeIndices_;
    /// @brief 要素の生ポインタからグローバルインデックスへの逆引き
    std::unordered_map<const T *, size_t> indexByPointer_;
    /// @brief これまでに割り当てたスロット数（フリーリスト分含む延べ数）
    size_t size_ = 0;
};

} // namespace KashipanEngine
