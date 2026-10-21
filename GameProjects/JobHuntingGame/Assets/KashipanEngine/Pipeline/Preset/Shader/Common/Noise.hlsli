#pragma once

float Rand2DTo1D(float2 seed) {
    float2 smallValue = sin(seed);
    float random = dot(smallValue, float2(12.9898f, 78.233f));
    random = frac(sin(random) * 143758.5453f);
    return random;
}
