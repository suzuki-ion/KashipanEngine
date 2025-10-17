#include "GameEngine.h"

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
/// @brief エンジン生成時のウィンドウタイトル
std::string sInitialWindowTitle = "KashipanEngine";
/// @brief エンジン生成時のウィンドウ幅
int32_t sInitialWindowWidth = 1280;
/// @brief エンジン生成時のウィンドウ高さ
int32_t sInitialWindowHeight = 720;
} // namespace

GameEngine::~GameEngine() {}

GameEngine::GameEngine(const std::string &title, int32_t width, int32_t height) {
    sInitialWindowTitle = title;
    sInitialWindowWidth = width;
    sInitialWindowHeight = height;
    windowsAPI_ = WindowsAPI::Factory::Create(
        sInitialWindowTitle,
        sInitialWindowWidth,
        sInitialWindowHeight);
    directXCommon_ = DirectXCommon::Factory::Create();
    mainWindow_ = windowsAPI_->CreateWindowInstance(title, width, height);
}

} // namespace KashipanEngine