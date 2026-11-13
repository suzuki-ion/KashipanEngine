#include "Splash/SplashAnimation.h"
#include <cstdlib>
#include <iostream>

using namespace KashipanEngine::SplashAnimation;
void Require(bool condition, const char *message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    Require(ToCanvas(0, 0).y == 1080 && ToCanvas(1920, 1080).y == 0, "world Y must point up");
    Require(ToCanvas(kLogoX, kTextY).y > ToCanvas(kLogoX, kLogoY).y, "text must appear below the gear");
    Require(ToCanvasRotation(90) == -90, "positive world rotation must be counterclockwise on screen");
    const auto fhd = ChooseClientSize(1920, 1080, 1904, 1001);
    Require(fhd.width == 1280 && fhd.height == 720, "FHD monitor must produce HD client");
    const auto uhd = ChooseClientSize(3840, 2160, 3808, 2000);
    Require(uhd.width == 2560 && uhd.height == 1440, "UHD monitor must scale client to QHD");
    const auto portrait = ChooseClientSize(1080, 1920, 1064, 1840);
    Require(portrait.width == 720 && portrait.height == 405, "portrait monitor must retain landscape canvas");
    const auto smallWork = ChooseClientSize(1920, 1080, 900, 500);
    Require(smallWork.width <= 900 && smallWork.height <= 500, "frame and work-area limits must be respected");
    const auto hdViewport = FitViewport(1280, 720);
    Require(std::abs(hdViewport.scale - 2.0f/3.0f) < 0.00001f && hdViewport.x == 0 && hdViewport.y == 0,
        "HD viewport must uniformly scale FHD coordinates");
    const auto squareViewport = FitViewport(1000, 1000);
    Require(std::abs(squareViewport.x) < 0.001f && std::abs(squareViewport.y - 218.75f) < 0.001f,
        "non-16:9 client must center canvas without stretching");
    const auto quarterTurn = ToCanvas(kLogoX, kLogoY + 256);
    Require(quarterTurn.y < ToCanvas(kLogoX, kLogoY).y, "rotated right-edge point must move upward");
    Require(Sample(0).gearScale == 0 && Sample(1).breadScale == 0, "intro must start hidden");
    Require(Sample(1.4).gearScale > 512, "intro back easing must overshoot");
    Require(Sample(1.75).textAlpha == 1 && Sample(2).wipeWidth == 0, "text reveal must complete");
    Require(Sample(1.5).wipeCenter == 448 && Sample(1.75).wipeCenter == 960,
        "wipe must reveal from FHD text left edge to its center");
    Require(Sample(2).wipeCenter == 1472, "wipe must end at FHD text right edge");
    Require(!Sample(2.99).loading && Sample(3).loading, "loading begins after intro");
    Require(Sample(3.7).gearRotation > 360, "waiting rotation must overshoot 360 degrees");
    Require(std::abs(Sample(4).gearRotation) < 0.001f, "each rotation takes one second");
    Require(!Sample(1000).finished && Sample(1000).fade == 0, "slow load must never fade prematurely");
    Require(!Sample(3, 0.2).loading && Sample(3.5, 0.2).fade == 0.5f, "early ready must finish intro then fade");
    Require(Sample(4, 0.2).finished, "early ready must close after intro and fade");
    Require(Sample(3.9, 3.2).fade == 0, "mid-cycle ready must not interrupt rotation");
    Require(Sample(4, 3.2).gearRotation == 0 && Sample(4, 3.2).fade == 0, "stop at complete revolution");
    Require(Sample(4.5, 3.2).fade == 0.5f && Sample(4.5, 3.2).gearRotation == 0, "gear stays still during fade");
    Require(Sample(5, 3.2).finished, "late ready must close after fade");
    Require(Sample(4.5, 4).fade == 0.5f, "exact-boundary ready must not add a revolution");
    Require(Sample(20, 8.1).finished, "delayed timer must catch up from completion timestamp");
    std::cout << "Splash animation regression passed\n";
}
