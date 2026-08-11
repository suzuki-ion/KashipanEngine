#pragma once

cbuffer MultiPassDitherAccumulateCB : register(b0) {
    float gInvPassCount;
    float3 padding;
};
