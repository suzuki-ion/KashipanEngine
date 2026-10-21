#pragma once

cbuffer TemporalBlendCB : register(b0) {
    float gHistoryWeight;
    float3 padding;
};
