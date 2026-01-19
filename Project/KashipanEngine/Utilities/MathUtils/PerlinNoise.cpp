#include "PerlinNoise.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace KashipanEngine {

PerlinNoise::PerlinNoise(uint32_t seed) {
    perm_.resize(512);
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);
    std::mt19937 engine(seed);
    std::shuffle(p.begin(), p.end(), engine);
    for (int i = 0; i < 256; ++i) {
        perm_[i] = p[i];
        perm_[i + 256] = p[i];
    }
}

int PerlinNoise::FastFloor(float x) {
    int xi = static_cast<int>(x);
    return (x < xi) ? (xi - 1) : xi;
}

float PerlinNoise::Fade(float t) {
    // 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float PerlinNoise::Grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    float res = ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    return res;
}

float PerlinNoise::GetValue(float x) const {
    return GetValue(x, 0.0f, 0.0f);
}

float PerlinNoise::GetValue(float x, float y) const {
    return GetValue(x, y, 0.0f);
}

float PerlinNoise::GetValue(float x, float y, float z) const {
    int X = FastFloor(x) & 255;
    int Y = FastFloor(y) & 255;
    int Z = FastFloor(z) & 255;

    float xf = x - FastFloor(x);
    float yf = y - FastFloor(y);
    float zf = z - FastFloor(z);

    float u = Fade(xf);
    float v = Fade(yf);
    float w = Fade(zf);

    int A  = perm_[X] + Y;
    int AA = perm_[A] + Z;
    int AB = perm_[A + 1] + Z;
    int B  = perm_[X + 1] + Y;
    int BA = perm_[B] + Z;
    int BB = perm_[B + 1] + Z;

    float x1, x2, y1, y2;

    x1 = Lerp(Grad(perm_[AA], xf, yf, zf), Grad(perm_[BA], xf - 1, yf, zf), u);
    x2 = Lerp(Grad(perm_[AB], xf, yf - 1, zf), Grad(perm_[BB], xf - 1, yf - 1, zf), u);
    y1 = Lerp(x1, x2, v);

    x1 = Lerp(Grad(perm_[AA + 1], xf, yf, zf - 1), Grad(perm_[BA + 1], xf - 1, yf, zf - 1), u);
    x2 = Lerp(Grad(perm_[AB + 1], xf, yf - 1, zf - 1), Grad(perm_[BB + 1], xf - 1, yf - 1, zf - 1), u);
    y2 = Lerp(x1, x2, v);

    float result = Lerp(y1, y2, w);
    return result;
}

} // namespace KashipanEngine
