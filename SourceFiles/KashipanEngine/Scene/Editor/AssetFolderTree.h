#pragma once
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "Debug/SceneChangeTiming.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {
/// メインスレッドが所有するフォルダーデータ。シーン・ImGui状態・エディターのコンテキストから独立する。
class AssetFolderTree final {
public:
    struct Node {
        std::string name;
        std::string path; // プロジェクト内の論理パス。UIのルートだけは空文字列。
        std::vector<std::unique_ptr<Node>> children;
    };
    static std::shared_ptr<AssetFolderTree> Acquire(const std::string &projectRoot,
        const std::string &assetsRoot, const std::string &logicalRoot, bool force = false) {
        auto &cached = Cache();
        if (!force && cached && cached->complete_ && cached->projectRoot_ == projectRoot &&
            cached->assetsRoot_ == assetsRoot && cached->logicalRoot_ == logicalRoot) {
            SceneLoadTiming::Scope timing("AssetsWindow.ReuseFolderTree");
            return cached;
        }
        SceneLoadTiming::Scope timing("AssetsWindow.BuildFolderTree");
        auto tree = std::shared_ptr<AssetFolderTree>(new AssetFolderTree);
        tree->projectRoot_ = projectRoot;
        tree->assetsRoot_ = assetsRoot;
        tree->logicalRoot_ = logicalRoot;
        tree->root_.name = "Assets";
        tree->Build(tree->root_, Utf8StringToPath(assetsRoot));
        std::sort(tree->folders_.begin(), tree->folders_.end());
        cached = tree; // 旧SceneEditorの破棄から新SceneEditorの生成まで保持する。
        return tree;
    }
    Node &Root() { return root_; }
    const std::vector<std::string> &Folders() const { return folders_; }
    bool Matches(const std::vector<std::string> &sortedFolders) const { return complete_ && folders_ == sortedFolders; }

    static std::string ChildPath(const std::string &parent, const std::string &name) {
        std::string result = parent;
        if (!result.empty() && result.back() != '/') result += '/';
        result += name;
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }
    /// Buildと同じ表示判定。既存のバックグラウンドでのアセット探索でも使用する。
    static bool IsVisibleFolder(const std::filesystem::path &relative, const std::filesystem::file_status &status) {
        if (!std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) return false;
        for (const auto &part : relative) {
            const auto &name = part.native();
            if (!name.empty() && name.front() == '.') return false;
        }
        return true;
    }
private:
    static std::shared_ptr<AssetFolderTree> &Cache() {
        static std::shared_ptr<AssetFolderTree> cached; // 1つのプロジェクトに限定し、生存期間とメモリ使用量に上限を設ける。
        return cached;
    }
    void Build(Node &node, const std::filesystem::path &physical) {
        SceneLoadTiming::Scope nodeTiming("AssetsWindow.BuildFolderNode.Inclusive", node.path.empty() ? "Assets" : node.path);
        std::error_code ec;
        auto it = std::filesystem::directory_iterator(physical, std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) { complete_ = false; return; }
        for (const auto end = std::filesystem::directory_iterator(); it != end; it.increment(ec)) {
            if (ec) { complete_ = false; break; }
            std::error_code statusError;
            const auto status = it->symlink_status(statusError);
            if (statusError) { complete_ = false; continue; }
            if (!IsVisibleFolder(it->path().filename(), status)) continue;
            auto child = std::make_unique<Node>();
            child->name = PathToUtf8String(it->path().filename());
            {
                SceneLoadTiming::Scope pathTiming("AssetsWindow.ResolveLogicalPath", child->name);
                child->path = ChildPath(node.path.empty() ? logicalRoot_ : node.path, child->name);
            }
            folders_.push_back(child->path);
            Build(*child, it->path());
            node.children.push_back(std::move(child));
        }
        if (ec) complete_ = false;
        std::sort(node.children.begin(), node.children.end(), [](const auto &a, const auto &b) { return a->name < b->name; });
    }
    Node root_;
    std::string projectRoot_, assetsRoot_, logicalRoot_;
    std::vector<std::string> folders_;
    bool complete_ = true;
};
} // namespace KashipanEngine
