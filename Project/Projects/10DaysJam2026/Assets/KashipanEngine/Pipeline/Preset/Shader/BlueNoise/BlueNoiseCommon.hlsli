#pragma once

static const float kPi = 3.14159265358979323846f;

// 32bit整数の一様混合ハッシュ（Thomas Wang mix）。ブルーノイズ生成は起動時に1回きりなので
// 高品質な暗号学的乱数である必要はなく、雪崩効果があれば十分
uint BlueNoiseHash(uint x) {
    x = (x ^ 61u) ^ (x >> 16);
    x *= 9u;
    x ^= x >> 4;
    x *= 0x27d4eb2du;
    x ^= x >> 15;
    return x;
}

float BlueNoiseRandomUnit(uint seed) {
    return float(BlueNoiseHash(seed)) / 4294967296.0f;
}
