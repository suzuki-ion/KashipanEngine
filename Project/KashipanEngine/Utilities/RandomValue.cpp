#include "RandomValue.h"
#include <random>

#include "Debug/Logger.h"

namespace KashipanEngine {

namespace {
std::mt19937 mtEngine; // メルセンヌ・ツイスタの乱数エンジン
/// @brief 乱数エンジンの初期化
class RandomEngineInitializer {
public:
    RandomEngineInitializer() {
        LogScope scope;
        std::random_device rd;
        mtEngine.seed(rd());
    }
} randomEngineInitializer;
} // namespace

int GetRandomInt(int min, int max) {
    LogScope scope;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mtEngine);
}

float GetRandomFloat(float min, float max) {
    LogScope scope;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(mtEngine);
}

double GetRandomDouble(double min, double max) {
    LogScope scope;
    std::uniform_real_distribution<double> dist(min, max);
    return dist(mtEngine);
}

int64_t GetRandomInt64(int64_t min, int64_t max) {
    LogScope scope;
    std::uniform_int_distribution<int64_t> dist(min, max);
    return dist(mtEngine);
}

bool GetRandomBool(float trueProbability) {
    LogScope scope;
    std::bernoulli_distribution dist(trueProbability);
    return dist(mtEngine);
}

} // namespace KashipanEngine