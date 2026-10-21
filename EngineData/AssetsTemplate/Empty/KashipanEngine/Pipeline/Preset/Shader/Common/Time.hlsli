#pragma once

cbuffer TimeConstants : register(b1) {
    float gTime;      // プログラム起動からの経過秒数（ポーズ中も進み続ける）
    float gDeltaTime;  // 前フレームからの経過秒数
    float2 padding;
};
