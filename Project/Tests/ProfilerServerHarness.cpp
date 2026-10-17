#include "Debug/ProfilerServer.h"
#include <iostream>

int main(int argc, char **argv) {
    const auto port = static_cast<unsigned short>(argc > 1 ? std::stoi(argv[1]) : 47181);
    KashipanEngine::ProfilerServer server(port);
    std::cout << server.Status() << std::endl;
    const int duration = argc > 2 ? std::stoi(argv[2]) : 5;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(duration);
    while (std::chrono::steady_clock::now() < until) {
        auto &profiler = KashipanEngine::Profiler::GetInstance();
        profiler.BeginFrame();
        {
            KashipanEngine::Profiler::Scope frame("Test.Frame");
            KashipanEngine::Profiler::Scope unicode("Test.日本語");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        profiler.EndFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(6));
    }
    std::cout << "Completed " << KashipanEngine::Profiler::GetInstance().GetSnapshot().frameId << std::endl;
}
