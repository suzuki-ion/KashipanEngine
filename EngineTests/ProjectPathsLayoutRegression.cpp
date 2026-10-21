// Exercise the production path resolver without creating windows or a D3D device.
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace KashipanEngine {
// Isolate logging/translations and JSON loading; path resolution itself is production code.
void Log(const std::string &, LogSeverity) {}
void LogScope::PushPrefix(const std::source_location &) {}
void LogScope::PopPrefix() {}
const std::string &GetTranslationText(const std::string &key) { return key; }
JSON LoadJSON(const std::string &path) {
    std::ifstream input(Utf8StringToPath(path));
    if (!input) return {};
    return JSON::parse(input);
}
DirectoryData GetDirectoryData(const std::string &, bool, bool) { return {}; }
DirectoryData GetDirectoryDataByExtension(const std::string &, const std::vector<std::string> &, bool, bool) { return {}; }
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int Execute(PasskeyForWinMain, const std::string &) {
    const auto bin = Utf8StringToPath(ProjectPaths::ExecutableDirectory());
    Require(Utf8StringToPath(ProjectPaths::WritableDirectory("BeforeProject")) == bin / "BeforeProject", "pre-project writable path");
    ProjectPaths::Initialize({});
    const auto root = Utf8StringToPath(ProjectPaths::EngineRoot());
    const auto project = Utf8StringToPath(ProjectPaths::ProjectRoot());
    Require(ProjectPaths::HasActiveProject(), "project discovery");
    Require(Utf8StringToPath(ProjectPaths::ProjectsRoot()) == root / "GameProjects", "GameProjects root");
    Require(Utf8StringToPath(ProjectPaths::AssetsTemplateRoot()) == root / "EngineData" / "AssetsTemplate", "template root");
    Require(Utf8StringToPath(ProjectPaths::InEngineData("Locales")) == root / "EngineData" / "Locales", "global locale root");
    Require(Utf8StringToPath(ProjectPaths::ToPhysical("Assets/Scenes/Test.scene")) == project / "Assets/Scenes/Test.scene", "asset path");
    Require(ProjectPaths::ToLogical(ProjectPaths::ToPhysical("Assets/Scenes/Test.scene")) == "Assets/Scenes/Test.scene", "logical round trip");
    Require(Utf8StringToPath(ProjectPaths::WritableDirectory("Logs")) == project / "Logs", "per-game logs");
    Require(Utf8StringToPath(ProjectPaths::WritableDirectory("Dumps")) == project / "Dumps", "per-game dumps");
    const bool standalone = wcsstr(GetCommandLineW(), L"--standalone-check") != nullptr;
    Require(ProjectPaths::IsStandalone() == standalone, "standalone detection");
    if (standalone) Require(root == bin && project == bin, "standalone roots");
    else Require(root == bin.parent_path() && project != bin, "editor roots independent of exe");
    std::cout << "ProjectPaths layout passed\n";
    return 0;
}
}
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        KashipanEngine::ProjectPaths::InitializeEngineRootOnly({});
        return KashipanEngine::Execute({}, "");
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
int main() { return WinMain(nullptr, nullptr, nullptr, 0); }
