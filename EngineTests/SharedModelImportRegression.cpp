#include "Assets/SharedModelImport.h"
#include <Windows.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
using KashipanEngine::SharedModelImport;
void CompareNodes(const aiNode *a, const aiNode *b) {
    assert(a && b && a->mName == b->mName && a->mNumChildren == b->mNumChildren);
    assert(a->mTransformation == b->mTransformation);
    for (unsigned int i = 0; i < a->mNumChildren; ++i) CompareNodes(a->mChildren[i], b->mChildren[i]);
}
int main() {
    const auto root = std::filesystem::absolute("Generated/SharedModelRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto source = root / L"三角形.obj";
    std::ofstream(source) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::weak_ptr<const SharedModelImport::Result> lifetime;
    {
        SharedModelImport::StartupSession session;
        std::vector<SharedModelImport::ResultPtr> results(8);
        std::vector<std::thread> workers;
        for (size_t i = 0; i < results.size(); ++i) workers.emplace_back([&, i] { results[i] = SharedModelImport::Load(source, SharedModelImport::CommonFlags); });
        for (auto &thread : workers) thread.join();
        for (const auto &result : results) assert(result == results[0] && result->scene && result->scene->mNumMeshes == 1);
        const auto [hits, misses] = SharedModelImport::Statistics();
        assert(hits == 7 && misses == 1);
        lifetime = results[0];
        results.clear();
        assert(!lifetime.expired()); // Session keeps the scene alive between managers.
        session.Close();
        assert(lifetime.expired());
    }
    auto a = SharedModelImport::Load(source, SharedModelImport::CommonFlags);
    auto b = SharedModelImport::Load(source, SharedModelImport::CommonFlags);
    assert(a != b); // Editor/dynamic imports are not cached after startup.
    std::ofstream(source) << "v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n";
    auto changed = SharedModelImport::Load(source, SharedModelImport::CommonFlags);
    assert(changed->scene->mMeshes[0]->mVertices[1].x != a->scene->mMeshes[0]->mVertices[1].x);
    {
        SharedModelImport::StartupSession session;
        auto missing = SharedModelImport::Load(root / "missing.obj", SharedModelImport::CommonFlags);
        assert(!missing->scene && !missing->error.empty());
        assert(SharedModelImport::Load(root / "missing.obj", SharedModelImport::CommonFlags) == missing);
    }
    // Compare actual startup models' hierarchy/bones/animation metadata with the original skeleton/animation import flags.
    for (const auto &file : {std::filesystem::path(L"GameProjects/Test/Assets/TestModels/Luchika/Luchika.gltf"),
        std::filesystem::path(L"GameProjects/Test/Assets/TestModels/Meepa/メェパ_FBX.fbx")}) {
        if (!std::filesystem::exists(file)) continue;
        auto original = SharedModelImport::Load(file, SharedModelImport::CommonFlags);
        SharedModelImport::StartupSession session;
        auto shared = SharedModelImport::Load(file, SharedModelImport::CommonFlags);
        assert(original->scene && shared->scene);
        CompareNodes(original->scene->mRootNode, shared->scene->mRootNode);
        assert(original->scene->mNumMeshes == shared->scene->mNumMeshes);
        assert(original->scene->mNumAnimations == shared->scene->mNumAnimations);
        for (unsigned int i = 0; i < original->scene->mNumMeshes; ++i) {
            const auto *x = original->scene->mMeshes[i], *y = shared->scene->mMeshes[i];
            assert(x->mNumBones == y->mNumBones);
            for (unsigned int j = 0; j < x->mNumBones; ++j) assert(x->mBones[j]->mName == y->mBones[j]->mName && x->mBones[j]->mOffsetMatrix == y->mBones[j]->mOffsetMatrix);
        }
        std::cout << "Model hierarchy/bone comparison passed\n";
    }
    // Exercise animation-bearing data too; the two startup fixtures contain no animation.
    const auto animatedSource = root / "animated.gltf";
    {
        std::vector<char> buffer(76, 0);
        const float positions[] = {0,0,0, 1,0,0, 0,1,0};
        const uint16_t indices[] = {0,1,2};
        const float times[] = {0,1};
        const float translations[] = {0,0,0, 1,0,0};
        std::memcpy(buffer.data(), positions, sizeof(positions));
        std::memcpy(buffer.data()+36, indices, sizeof(indices));
        std::memcpy(buffer.data()+44, times, sizeof(times));
        std::memcpy(buffer.data()+52, translations, sizeof(translations));
        std::ofstream binary(root / "animated.bin", std::ios::binary);
        binary.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        binary.close();
        std::ofstream(animatedSource) << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"Mover","mesh":0}],"buffers":[{"uri":"animated.bin","byteLength":76}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":8},{"buffer":0,"byteOffset":52,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},{"bufferView":3,"componentType":5126,"count":2,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"animations":[{"samplers":[{"input":2,"output":3,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}]})";
        auto original = SharedModelImport::Load(animatedSource, SharedModelImport::CommonFlags);
        SharedModelImport::StartupSession session(root / "animated-cache");
        auto shared = SharedModelImport::Load(animatedSource, SharedModelImport::CommonFlags);
        assert(original->scene && shared->scene && original->scene->mNumAnimations == 1 && shared->scene->mNumAnimations == 1);
        const auto *x = original->scene->mAnimations[0], *y = shared->scene->mAnimations[0];
        assert(x->mDuration == y->mDuration && x->mTicksPerSecond == y->mTicksPerSecond && x->mNumChannels == y->mNumChannels);
        for (unsigned int i=0; i<x->mNumChannels; ++i) {
            const auto *cx=x->mChannels[i], *cy=y->mChannels[i];
            assert(cx->mNodeName == cy->mNodeName && cx->mNumPositionKeys == cy->mNumPositionKeys);
            for (unsigned int j=0; j<cx->mNumPositionKeys; ++j) assert(cx->mPositionKeys[j].mTime == cy->mPositionKeys[j].mTime && cx->mPositionKeys[j].mValue == cy->mPositionKeys[j].mValue);
        }
        session.Close();
        {
            SharedModelImport::StartupSession warmSession(root / "animated-cache");
            auto restored = SharedModelImport::Load(animatedSource, SharedModelImport::CommonFlags);
            assert(restored->diskCacheHit && restored->scene);
            const auto *keys = restored->scene->mAnimations[0]->mChannels[0];
            assert(keys->mNumPositionKeys == y->mChannels[0]->mNumPositionKeys);
            for (unsigned int k = 0; k < keys->mNumPositionKeys; ++k)
                assert(keys->mPositionKeys[k].mTime == y->mChannels[0]->mPositionKeys[k].mTime && keys->mPositionKeys[k].mValue == y->mChannels[0]->mPositionKeys[k].mValue);
        }
        // External glTF buffer changes must invalidate even with unchanged size and timestamp.
        const auto binaryPath = root / "animated.bin";
        const auto stamp = std::filesystem::last_write_time(binaryPath);
        { std::fstream changedBuffer(binaryPath, std::ios::binary | std::ios::in | std::ios::out);
          changedBuffer.seekp(64); const float changedX = 3.0f; changedBuffer.write(reinterpret_cast<const char *>(&changedX), sizeof(changedX)); }
        std::filesystem::last_write_time(binaryPath, stamp);
        {
            SharedModelImport::StartupSession changedSession(root / "animated-cache");
            auto changedBuffer = SharedModelImport::Load(animatedSource, SharedModelImport::CommonFlags);
            assert(changedBuffer->scene && !changedBuffer->diskCacheHit);
        }
        std::cout << "Animated model key/cache/buffer invalidation comparison passed\n";
    }
    a.reset(); b.reset(); changed.reset();
    std::filesystem::remove_all(root); // Newly created, process-specific directory under Generated.
    std::cout << "Shared model import regression passed\n";
}
