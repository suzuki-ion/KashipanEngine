#include "Debug/StartupTiming.h"
#include <cassert>
#include <iostream>

namespace {
std::vector<std::string> lines;
}
namespace KashipanEngine {
void Log(const std::string &text, LogSeverity severity) {
    assert(severity == LogSeverity::Info);
    lines.push_back(text);
}
}

int main() {
    using KashipanEngine::StartupTiming;
    StartupTiming::Sections idle("Idle.");
    idle.End("BeforeStartup");
    assert(lines.empty());

    StartupTiming::Begin();
    StartupTiming::Sections sections("Test.");
    sections.End("BeforeLogger");
    assert(lines.empty());
    StartupTiming::EnableLogging();
    assert(lines.size() == 1);
    assert(lines.back().find("[StartupTiming] Test.BeforeLogger:") == 0);
    assert(lines.back().find(" ms (") != std::string::npos);
    StartupTiming::EnableLogging();
    assert(lines.size() == 1); // バッファーに保持した行は一度だけ出力する。
    sections.End("AfterLogger");
    StartupTiming::Milestone("Startup.InitializationTotal");
    assert(lines.size() == 3);
    assert(StartupTiming::IsActive()); // 初期化とシーンの初回描画は別々に計測する。
    StartupTiming::Complete();
    assert(!StartupTiming::IsActive());
    assert(lines.size() == 4);
    assert(lines.back().find("Startup.Total") != std::string::npos);
    sections.End("AfterStartup");
    StartupTiming::Complete();
    StartupTiming::Interrupt();
    assert(lines.size() == 4); // 起動完了後はフレームごとのログを出力しない。

    StartupTiming::Begin();
    sections.End("NewSession");
    assert(lines.size() == 4); // Begin はロガーの準備状態をリセットする。
    StartupTiming::EnableLogging();
    StartupTiming::Interrupt();
    assert(lines.size() == 6);
    assert(lines.back().find("Startup.Interrupted") != std::string::npos);
    assert(!StartupTiming::IsActive());
    std::cout << "Startup timing regression passed\n";
}
