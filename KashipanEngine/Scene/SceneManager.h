#pragma once

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace KashipanEngine {

class SceneBase;

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager &) = delete;
    SceneManager &operator=(const SceneManager &) = delete;
    SceneManager(SceneManager &&) = delete;
    SceneManager &operator=(SceneManager &&) = delete;

    template<typename TScene, typename... Args>
    void RegisterScene(const std::string &sceneName, Args &&...args) {
        static_assert(std::is_base_of_v<SceneBase, TScene>, "TScene must derive from SceneBase");

        factoriesByName_[sceneName] =
            [captured = std::make_tuple(std::forward<Args>(args)...)](SceneManager *sm) mutable -> std::unique_ptr<SceneBase> {
                auto scene = std::apply(
                    [](auto &&...unpacked) -> std::unique_ptr<SceneBase> {
                        return std::make_unique<TScene>(std::forward<decltype(unpacked)>(unpacked)...);
                    },
                    std::move(captured));

                if (scene) {
                    scene->SetSceneManager(Passkey<SceneManager>{}, sm);
                }
                return scene;
            };
    }

    SceneBase *GetCurrentScene() const {
        if (currentScene_) return currentScene_.get();
        return currentSceneLegacy_;
    }

    bool ChangeScene(const std::string &sceneName);

private:
    using SceneFactory = std::function<std::unique_ptr<SceneBase>(SceneManager *)>;

    std::unordered_map<std::string, SceneFactory> factoriesByName_;

    std::unique_ptr<SceneBase> currentScene_;
    SceneBase *currentSceneLegacy_ = nullptr;
};

} // namespace KashipanEngine
