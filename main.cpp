#include <KashipanEngine.h>
#include <iostream>

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::unique_ptr<KashipanEngine::GameEngine> engine;
    engine = std::make_unique<KashipanEngine::GameEngine>("Kashipan Engine Window", 800, 600);
    return 0;
}