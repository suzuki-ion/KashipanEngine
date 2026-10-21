#include "Graphics/TextureUploadBatch.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
int main() {
    using KashipanEngine::TextureUploadBatch;
    size_t waits = 0, records = 0;
    std::weak_ptr<int> retained;
    TextureUploadBatch batch([&](const TextureUploadBatch::Task &record) {
        record(nullptr);
        assert(!retained.expired()); // Upload memory must survive through the GPU wait.
        ++waits;
    }, 10);
    auto upload = std::make_shared<int>(42);
    retained = upload;
    batch.Add(4, [upload, &records](ID3D12GraphicsCommandList *) { assert(*upload == 42); ++records; });
    upload.reset();
    batch.Add(4, [&records](ID3D12GraphicsCommandList *) { ++records; });
    assert(waits == 0 && !retained.expired());
    batch.Flush();
    assert(records == 2 && waits == 1 && retained.expired());
    batch.Flush();
    assert(waits == 1);
    size_t boundedWaits = 0;
    TextureUploadBatch bounded([&](const TextureUploadBatch::Task &record) { record(nullptr); ++boundedWaits; }, 10);
    const auto empty = [](ID3D12GraphicsCommandList *) {};
    bounded.Add(6, empty);
    bounded.Add(6, empty); // Flush before exceeding the limit.
    assert(boundedWaits == 1);
    bounded.Add(20, empty); // Flush the remainder, then submit the large image alone.
    assert(boundedWaits == 3 && bounded.TextureCount() == 3);
    bounded.Flush();
    assert(boundedWaits == 3);
    auto failureResource = std::make_shared<int>(1);
    std::weak_ptr<int> failureRetained = failureResource;
    TextureUploadBatch failed([](const TextureUploadBatch::Task &) { throw std::runtime_error("submit"); });
    failed.Add(1, [failureResource](ID3D12GraphicsCommandList *) {});
    failureResource.reset();
    try { failed.Flush(); assert(false); } catch (const std::runtime_error &) {}
    assert(!failureRetained.expired() && failed.SubmissionCount() == 0);
    std::cout << "Texture upload batch regression passed\n";
}
