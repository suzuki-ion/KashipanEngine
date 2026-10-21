#include "Scene/Editor/AssetFolderTree.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
namespace KashipanEngine { void Log(const std::string &, LogSeverity) {} }
using namespace KashipanEngine;
namespace {
using Clock = std::chrono::steady_clock;
std::string Text(const std::filesystem::path &p) {
    auto text = PathToUtf8String(p);
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}
double Milliseconds(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
void LegacyFolders(const std::filesystem::path &physical, const std::filesystem::path &project, std::vector<std::string> &paths) {
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(physical,std::filesystem::directory_options::skip_permission_denied,ec)) {
        const auto status = entry.symlink_status(ec);
        if (ec || !std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) continue;
        const auto name = PathToUtf8String(entry.path().filename());
        if (!name.empty() && name.front()=='.') continue;
        const auto relative = std::filesystem::relative(entry.path(),project,ec);
        paths.push_back(ec ? Text(entry.path()) : Text(relative));
        LegacyFolders(entry.path(),project,paths);
    }
}
std::vector<std::string> Legacy(const std::filesystem::path &project) {
    std::vector<std::string> paths;
    LegacyFolders(project/"Assets",project,paths);
    std::sort(paths.begin(),paths.end());
    return paths;
}
}
int main(int argc,char **argv) {
    const auto root = std::filesystem::absolute("Generated/AssetFolderTreeRegression-"+
        std::to_string(Clock::now().time_since_epoch().count()));
    const auto project = root/"ProjectA";
    const auto assets = project/"Assets";
    std::filesystem::create_directories(assets/"Folder/Sub");
    std::filesystem::create_directories(assets/"Empty");
    std::filesystem::create_directories(assets/Utf8StringToPath("日本語"));
    std::filesystem::create_directories(assets/".hidden/Child");
    std::ofstream(assets/"Folder/readme.txt") << "first";
    assert(AssetFolderTree::ChildPath("Assets/", "Folder\\Sub")=="Assets/Folder/Sub");
    assert(!AssetFolderTree::IsVisibleFolder(".hidden/Child",std::filesystem::file_status(std::filesystem::file_type::directory)));
    assert(!AssetFolderTree::IsVisibleFolder("Link",std::filesystem::file_status(std::filesystem::file_type::symlink)));
    auto first = AssetFolderTree::Acquire(Text(project),Text(assets),"Assets");
    assert(first->Root().path.empty() && first->Folders().size()==4);
    assert(first->Matches(Legacy(project)));
    const auto *identity = first.get();
    first.reset(); // Shared data must survive destruction of the old window/scene.
    auto second = AssetFolderTree::Acquire(Text(project),Text(assets),"Assets");
    assert(second.get()==identity);
    std::ofstream(assets/"Folder/readme.txt") << "changed contents";
    assert(second->Matches(Legacy(project))); // A content edit does not invalidate the folder tree.
    std::filesystem::create_directory(assets/"Later");
    const auto added = Legacy(project);
    assert(!second->Matches(added)); // Empty directories are part of the snapshot too.
    auto refreshed = AssetFolderTree::Acquire(Text(project),Text(assets),"Assets",true);
    assert(refreshed.get()!=second.get() && refreshed->Matches(added));
    assert(second->Folders().size()==4); // A live old view is not mutated by a replacement tree.
    std::filesystem::rename(assets/"Later",assets/"Renamed");
    assert(!refreshed->Matches(Legacy(project)));
    refreshed = AssetFolderTree::Acquire(Text(project),Text(assets),"Assets",true);
    std::filesystem::remove(assets/"Empty");
    assert(!refreshed->Matches(Legacy(project)));
    const auto other = root/"ProjectB";
    std::filesystem::create_directories(other/"Assets/OnlyB");
    auto differentProject = AssetFolderTree::Acquire(Text(other),Text(other/"Assets"),"Assets");
    assert(differentProject.get()!=refreshed.get());
    assert(differentProject->Folders()==std::vector<std::string>{"Assets/OnlyB"});
    auto switchedBack = AssetFolderTree::Acquire(Text(project),Text(assets),"Assets");
    assert(switchedBack.get()!=differentProject.get() && switchedBack->Matches(Legacy(project)));
    auto otherLogicalRoot = AssetFolderTree::Acquire(Text(project),Text(assets),"Other");
    assert(otherLogicalRoot.get()!=switchedBack.get());
    assert(otherLogicalRoot->Folders().front().starts_with("Other/"));
    std::cout << "Asset folder tree regression passed: paths, Unicode, hidden/link filtering, lifetime/reuse, add/rename/delete, force refresh, project isolation\n";
    if (argc>1) {
        const auto realProject = std::filesystem::absolute(Utf8StringToPath(argv[1]));
        for (int trial=0; trial<3; ++trial) {
            std::vector<std::string> expected;
            std::shared_ptr<AssetFolderTree> actual;
            double legacyMs=0,buildMs=0;
            const auto old = [&] { const auto start=Clock::now(); expected=Legacy(realProject); legacyMs=Milliseconds(start); };
            const auto fresh = [&] { const auto start=Clock::now(); actual=AssetFolderTree::Acquire(Text(realProject),Text(realProject/"Assets"),"Assets",true); buildMs=Milliseconds(start); };
            if (trial%2) { fresh(); old(); } else { old(); fresh(); }
            assert(actual->Matches(expected));
            const auto start=Clock::now();
            auto reused=AssetFolderTree::Acquire(Text(realProject),Text(realProject/"Assets"),"Assets");
            const double reuseMs=Milliseconds(start);
            assert(reused.get()==actual.get());
            std::cout << "BENCH trial=" << trial+1 << " folders=" << expected.size() << " legacyMs=" << legacyMs << " buildMs=" << buildMs << " reuseMs=" << reuseMs << '\n';
        }
    }
}
