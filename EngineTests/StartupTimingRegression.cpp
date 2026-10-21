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
    assert(lines.size() == 1); // Buffered lines are flushed exactly once.
    sections.End("AfterLogger");
    StartupTiming::Milestone("Startup.InitializationTotal");
    assert(lines.size() == 3);
    assert(StartupTiming::IsActive()); // Initialization is not first scene draw.
    StartupTiming::Complete();
    assert(!StartupTiming::IsActive());
    assert(lines.size() == 4);
    assert(lines.back().find("Startup.Total") != std::string::npos);
    sections.End("AfterStartup");
    StartupTiming::Complete();
    StartupTiming::Interrupt();
    assert(lines.size() == 4); // No per-frame logs after startup completes.

    StartupTiming::Begin();
    sections.End("NewSession");
    assert(lines.size() == 4); // Begin resets logger readiness.
    StartupTiming::EnableLogging();
    StartupTiming::Interrupt();
    assert(lines.size() == 6);
    assert(lines.back().find("Startup.Interrupted") != std::string::npos);
    assert(!StartupTiming::IsActive());
    std::cout << "Startup timing regression passed\n";
}
