#include "RandomValue.h"
#include <random>

namespace KashipanEngine {

namespace {
std::mt19937 mtEngine; // メルセンヌ・ツイスタの乱数エンジン
/// @brief 乱数エンジンの初期化
class RandomEngineInitializer {
public:
    RandomEngineInitializer() {
        std::random_device rd;
        mtEngine.seed(rd());
    }
} randomEngineInitializer;
} // namespace

int GetRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mtEngine);
}

float GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(mtEngine);
}

double GetRandomDouble(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(mtEngine);
}

bool GetRandomBool(float trueProbability) {
    std::bernoulli_distribution dist(trueProbability);
    return dist(mtEngine);
}

} // namespace KashipanEngine