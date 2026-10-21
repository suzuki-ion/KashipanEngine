#pragma once
#include <filesystem>
#include <string>
namespace KashipanEngine::DevelopmentAI {
struct ApiContext {
    std::string text;
    size_t sourceFiles = 0, referenceFiles = 0;
};
// Host-side, bounded retrieval. Never grants an agent write access to engine sources/references.
ApiContext BuildApiContext(const std::filesystem::path &engineRoot, const std::string &request, size_t maximumBytes = 80000);
}
