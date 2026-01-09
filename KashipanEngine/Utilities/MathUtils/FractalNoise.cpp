#include "FractalNoise.h"
#include <cmath>

namespace KashipanEngine {

FractalNoise::FractalNoise(uint32_t seed)
    : pn_(seed) {
}

float FractalNoise::GetValue(float x, float y, float z, int octaves, float lacunarity, float persistence) const {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float maxAmp = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        sum += amplitude * pn_.GetValue(x * frequency, y * frequency, z * frequency);
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    if (maxAmp == 0.0f) return 0.0f;
    return sum / maxAmp;
}

float FractalNoise::GetValue(float x, float y, int octaves, float lacunarity, float persistence) const {
    return GetValue(x, y, 0.0f, octaves, lacunarity, persistence);
}

float FractalNoise::GetValue(float x, int octaves, float lacunarity, float persistence) const {
    return GetValue(x, 0.0f, 0.0f, octaves, lacunarity, persistence);
}

} // namespace KashipanEngine
