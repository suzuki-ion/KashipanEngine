#include "Debug/Profiler.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using KashipanEngine::Profiler;
void Require(bool value) { if (!value) throw std::runtime_error("Profiler regression failed"); }
const Profiler::Result &Find(const Profiler::Snapshot &snapshot, const char *name) {
    for (const auto &result : snapshot.results) if (result.name == name) return result;
    throw std::runtime_error("Missing profiling result");
}
int main() {
    auto &profiler = Profiler::GetInstance();
    Require(profiler.GetSnapshot().results.empty());
    profiler.SetSampleCount(2);
    profiler.BeginFrame();
    {
        Profiler::Scope outer("Outer");
        { Profiler::Scope inner("Inner"); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        { Profiler::Scope inner("Inner"); }
        std::thread worker([] { Profiler::Scope scope("Worker"); });
        worker.join();
    }
    Require(profiler.GetSnapshot().results.empty()); // partial frames never appear
    profiler.EndFrame();
    auto first = profiler.GetSnapshot();
    Require(first.frames == 1 && first.fps > 0 && first.frameMs > 0);
    Require(std::abs(first.fps * first.frameMs - 1000) < 0.000001);
    Require(Find(first, "Inner").calls == 2 && Find(first, "Worker").calls == 1);
    Require(Find(first, "Outer").milliseconds >= Find(first, "Inner").milliseconds);
    const double innerMs = Find(first, "Inner").milliseconds;
    profiler.BeginFrame();
    profiler.EndFrame();
    auto second = profiler.GetSnapshot();
    Require(second.frames == 2 && second.fps > 0);
    Require(Find(second, "Inner").calls == 0 && Find(second, "Inner").milliseconds == 0);
    Require(std::abs(Find(second, "Inner").averageMs - innerMs / 2) < 0.000001);
    Require(Find(second, "Inner").peakMs == innerMs);
    profiler.BeginFrame();
    auto stale = std::make_unique<Profiler::Scope>("Stale");
    profiler.EndFrame();
    profiler.BeginFrame();
    stale.reset();
    profiler.EndFrame();
    Require(profiler.GetSnapshot().results.empty()); // history expired; crossing scope discarded
    profiler.SetSampleCount(0); // clamped to one
    profiler.BeginFrame();
    { Profiler::Scope scope("Recent"); }
    profiler.EndFrame();
    Require(profiler.GetSnapshot().frames == 1 && Find(profiler.GetSnapshot(), "Recent").calls == 1);
    std::cout << "Profiler regression passed\n";
}
