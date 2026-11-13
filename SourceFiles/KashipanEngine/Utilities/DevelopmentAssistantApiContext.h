#pragma once
#include <filesystem>
#include <string>
namespace KashipanEngine::DevelopmentAI {
struct ApiContext {
    std::string text;
    size_t sourceFiles = 0, referenceFiles = 0;
};
// ホスト側で件数を制限して検索する。エージェントにエンジンソースやリファレンスの書き込み権限を与えない。
ApiContext BuildApiContext(const std::filesystem::path &engineRoot, const std::string &request, size_t maximumBytes = 80000);
}
