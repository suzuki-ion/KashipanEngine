#pragma once
#include "Assets/AssimpUtf8IOSystem.h"
#include "Assets/ModelSceneCache.h"
#include <atomic>
#include <unordered_set>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace KashipanEngine {
/// Import once during the bounded startup session, retain the owning Importer
/// until all three managers have copied their data. A persistent startup cache
/// skips source parsing on subsequent launches. Dynamic imports preserve their original path.
class SharedModelImport final {
public:
    struct Result {
        std::unique_ptr<Assimp::Importer> importer;
        std::unique_ptr<aiScene> cachedScene;
        bool diskCacheHit = false;
        const aiScene *scene = nullptr;
        std::string error;
    };
    using ResultPtr = std::shared_ptr<const Result>;
    static constexpr unsigned int CommonFlags = aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder |
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
        aiProcess_RemoveRedundantMaterials | aiProcess_FindInvalidData | aiProcess_TransformUVCoords |
        aiProcess_SortByPType | aiProcess_FlipUVs;
    static constexpr unsigned int ModelFlags = CommonFlags | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

    class StartupSession final {
    public:
        explicit StartupSession(std::filesystem::path cacheDirectory = {}) {
            std::lock_guard lock(mutex_);
            entries_.clear(); hits_ = misses_ = 0; active_ = true;
            cacheDirectory_ = std::move(cacheDirectory);
            diskHits_ = parses_ = stores_ = 0;
        }
        ~StartupSession() { Close(); }
        StartupSession(const StartupSession &) = delete;
        StartupSession &operator=(const StartupSession &) = delete;
        void Close() {
            if (closed_) return;
            std::lock_guard lock(mutex_);
            active_ = false;
            entries_.clear();
            cacheDirectory_.clear();
            closed_ = true;
        }
    private:
        bool closed_ = false;
    };
    static std::pair<size_t, size_t> Statistics() {
        std::lock_guard lock(mutex_);
        return {hits_, misses_};
    }
    struct DiskStatistics { size_t hits, parsed, stored; };
    static DiskStatistics CacheStatistics() { return {diskHits_.load(), parses_.load(), stores_.load()}; }
    static ResultPtr Load(const std::filesystem::path &path, unsigned int originalFlags) {
        std::shared_future<ResultPtr> future;
        std::shared_ptr<std::promise<ResultPtr>> promise;
        bool active;
        std::filesystem::path cacheDirectory;
        {
            std::lock_guard lock(mutex_);
            active = active_;
            cacheDirectory = cacheDirectory_;
            if (active) {
                const auto key = PathToUtf8String(std::filesystem::absolute(path).lexically_normal());
                const auto found = entries_.find(key);
                if (found != entries_.end()) { future = found->second; ++hits_; }
                else {
                    promise = std::make_shared<std::promise<ResultPtr>>();
                    future = promise->get_future().share();
                    entries_.emplace(key, future);
                    ++misses_;
                }
            }
        }
        if (!active) return Import(path, originalFlags); // Preserve existing dynamic-load processing flags.
        if (promise) {
            try { promise->set_value(Import(path, ModelFlags, cacheDirectory)); }
            catch (...) { promise->set_exception(std::current_exception()); }
        }
        // Import work happens outside the mutex: different files remain parallel.
        return future.get();
    }
private:
    static ResultPtr Import(const std::filesystem::path &path, unsigned int flags,
        const std::filesystem::path &directory = {}) {
        auto result = std::make_shared<Result>();
        std::string key;
        std::filesystem::path cachePath;
        if (!directory.empty()) {
            try {
                key = ModelSceneCache::Key(path, flags);
                cachePath = ModelSceneCache::CachePath(directory, path, flags);
                if (!ModelSceneCache::LibraryIdentity().empty()) {
                    if (const auto bytes = ContentCache::Load(cachePath, key)) {
                        result->cachedScene = ModelSceneCache::Decode(*bytes);
                        if (result->cachedScene) {
                            result->scene = result->cachedScene.get();
                            result->diskCacheHit = true; ++diskHits_;
                            return result;
                        }
                    }
                }
            } catch (...) { key.clear(); }
        }
        ContentCache::Json dependencies = ContentCache::Json::array();
        std::unordered_set<std::string> observed;
        bool cacheable = !key.empty() && !ModelSceneCache::LibraryIdentity().empty();
        const auto observe = [&](const char *file, bool exists) {
            if (!cacheable) return;
            try {
                auto dependencyPath = std::filesystem::absolute(Utf8StringToPath(file)).lexically_normal();
                const auto identity = ContentCache::PathText(dependencyPath);
                if (!observed.insert(identity).second) return;
                if (exists) {
                    auto dependency = ContentCache::Dependency(dependencyPath);
                    if (dependency.at("hash").get<std::string>().empty()) { cacheable = false; return; }
                    dependencies.push_back(std::move(dependency));
                } else dependencies.push_back({{"path", identity}, {"missing", true}});
            } catch (...) { cacheable = false; }
        };
        observe(PathToUtf8String(path).c_str(), std::filesystem::exists(path));
        result->importer = std::make_unique<Assimp::Importer>();
        // The observer is needed only during ReadFile: never retain references to its stack frame.
        result->importer->SetIOHandler(new AssimpUtf8IOSystem(observe));
        result->scene = result->importer->ReadFile(PathToUtf8String(path), flags);
        result->importer->SetIOHandler(new AssimpUtf8IOSystem());
        if (!directory.empty()) ++parses_;
        if (!result->scene) result->error = result->importer->GetErrorString();
        else if (cacheable) {
            try {
                const auto payload = ModelSceneCache::Encode(*result->scene);
                if (ContentCache::Store(cachePath, key, dependencies, payload.data(), payload.size())) ++stores_;
            } catch (...) {} // Import succeeds even on a read-only disk or unsupported cache data.
        }
        return result;
    }
    inline static std::mutex mutex_;
    inline static std::filesystem::path cacheDirectory_;
    inline static std::atomic_size_t diskHits_{0}, parses_{0}, stores_{0};
    inline static bool active_ = false;
    inline static size_t hits_ = 0, misses_ = 0;
    inline static std::unordered_map<std::string, std::shared_future<ResultPtr>> entries_;
};
} // namespace KashipanEngine
