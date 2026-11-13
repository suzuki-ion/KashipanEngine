#pragma once
#include <d3d12.h>
#include <functional>
#include <vector>
#include <cstdint>
#include <utility>

namespace KashipanEngine {
/// 同期実行側がGPUの完了を待ち終えるまで、転送用のコールバックとリソースを
/// 所有する。大規模プロジェクトでも保持するアップロード用メモリに上限を設ける。
class TextureUploadBatch final {
public:
    using Task = std::function<void(ID3D12GraphicsCommandList *)>;
    using Executor = std::function<void(const Task &)>;
    explicit TextureUploadBatch(Executor executor, uint64_t limit = 64ULL * 1024 * 1024)
        : executor_(std::move(executor)), limit_(limit) {}
    void Add(uint64_t bytes, Task task) {
        if (!tasks_.empty() && (bytes >= limit_ || bytes > limit_ - bytes_)) Flush();
        tasks_.push_back(std::move(task));
        bytes_ += bytes;
        ++textureCount_;
        if (bytes_ >= limit_) Flush(); // 単独で上限を超える大きな画像は、それだけで送信する。
    }
    void Flush() {
        if (tasks_.empty()) return;
        executor_([this](ID3D12GraphicsCommandList *list) {
            for (const auto &task : tasks_) task(list);
        });
        // 実行側は同期処理を保証するため、この時点で初めてアップロード用リソースを解放できる。
        tasks_.clear();
        bytes_ = 0;
        ++submissionCount_;
    }
    size_t TextureCount() const { return textureCount_; }
    size_t SubmissionCount() const { return submissionCount_; }
private:
    Executor executor_;
    uint64_t limit_;
    uint64_t bytes_ = 0;
    size_t textureCount_ = 0;
    size_t submissionCount_ = 0;
    std::vector<Task> tasks_;
};
} // namespace KashipanEngine
