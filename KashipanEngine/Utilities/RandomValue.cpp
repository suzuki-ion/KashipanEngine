#include "RandomValue.h"
#include <random>

namespace KashipanEngine {

namespace {
std::mt19937 mtEngine; // メルセンヌ・ツイスタの乱数エンジン
} // namespace

void InitializeRandom() {
    std::random_device rd;
    mtEngine.seed(rd());
}

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