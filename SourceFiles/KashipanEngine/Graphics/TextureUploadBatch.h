#pragma once
#include <d3d12.h>
#include <functional>
#include <vector>
#include <cstdint>
#include <utility>

namespace KashipanEngine {
/// Owns transfer callbacks/resources until the synchronous executor has waited
/// for GPU completion. Bound the retained upload memory, even for large projects.
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
        if (bytes_ >= limit_) Flush(); // A single large image is submitted alone.
    }
    void Flush() {
        if (tasks_.empty()) return;
        executor_([this](ID3D12GraphicsCommandList *list) {
            for (const auto &task : tasks_) task(list);
        });
        // The executor contract is synchronous; only now can uploads be released.
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
