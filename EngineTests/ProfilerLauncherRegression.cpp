#include "Debug/ProfilerLauncher.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using KashipanEngine::ProfilerLauncher;
void Require(bool value) { if (!value) throw std::runtime_error("Profiler launcher regression failed"); }
void Wait(ProfilerLauncher &launcher) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (launcher.IsRunning() && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(!launcher.IsRunning());
}
int wmain(int argc, wchar_t **argv) {
    // Fake interpreter: real subprocess/argument parsing without opening a GUI.
    if (argc > 1) {
        if (argc != 4 || std::wstring(argv[2]) != L"--port" || !std::filesystem::is_regular_file(argv[1])) return 8;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::wstring(argv[3]) == L"47182" ? 7 : 0;
    }
    wchar_t executable[32768]{};
    Require(GetModuleFileNameW(nullptr, executable, 32768) != 0);
    const auto root = std::filesystem::current_path() / "Generated" /
        (L"Profiler Launcher 日本語 " + std::to_wstring(GetCurrentProcessId()));
    const auto directory = root / "EngineTools" / "Profiler";
    std::filesystem::create_directories(directory);
    const auto script = directory / "profiler.py";
    std::ofstream(script) << "# test fixture\n";
    Require(ProfilerLauncher::FindScript(root, root / "missing") == script);
    Require(ProfilerLauncher::FindScript(root / "missing", root) == script);
    Require(ProfilerLauncher::FindScript(root / "missing", root / "missing").empty());
    ProfilerLauncher launcher;
    Require(launcher.StartProcess(script, executable, false, 47181));
    Require(launcher.IsRunning());
    Require(launcher.StartProcess(script, executable, false, 47182)); // must keep first process
    Wait(launcher);
    Require(!launcher.TakeError());
    Require(launcher.StartProcess(script, executable, false, 47182));
    Wait(launcher);
    Require(launcher.TakeError() && launcher.ErrorCode() == 7);
    Require(!launcher.TakeError());
    Require(!launcher.StartProcess(script, root / "missing.exe", false, 47181));
    Require(launcher.TakeError() && launcher.ErrorCode() != 0);
    char previous[32768]{};
    const DWORD previousSize = GetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", previous, sizeof(previous));
    SetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", "47181");
    Require(ProfilerLauncher::Port() == 47181);
    // Exercise the same interpreter discovery and port arguments as the menu,
    // using installed Python and a short script that creates no window.
    std::ofstream(script) << "import pathlib, sys\npathlib.Path(__file__).with_suffix('.result').write_text(' '.join(sys.argv[1:]))\n";
    Require(launcher.Start(root, root));
    Wait(launcher);
    Require(!launcher.TakeError());
    auto resultFile = script; resultFile.replace_extension(".result");
    std::string arguments;
    std::ifstream result(resultFile); std::getline(result, arguments); result.close();
    Require(arguments == "--port 47181");
    std::filesystem::remove(resultFile);
    SetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", "0");
    Require(!launcher.Start(root, root));
    Require(std::string(launcher.ErrorKey()) == "editor.profiler.disabled");
    SetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", "bad");
    Require(ProfilerLauncher::Port() == 0);
    SetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", nullptr);
    Require(ProfilerLauncher::Port() == 47180);
    SetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", previousSize ? previous : nullptr);
    std::filesystem::remove(script);
    std::filesystem::remove(directory);
    std::filesystem::remove(directory.parent_path());
    std::filesystem::remove(root);
    std::cout << "Profiler launcher passed: installed Python, Unicode/spaces, duplicate launch, exit failure, paths and port\n";
}
