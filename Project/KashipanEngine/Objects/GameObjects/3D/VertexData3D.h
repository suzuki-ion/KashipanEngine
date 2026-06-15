#pragma once
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

struct VertexData3D {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    uint32_t boneIndices[4] = { 0, 0, 0, 0 };
    float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t enableSkinning = 0; // 0: スキニングなし、1: スキニングあり
};

} // namespace KashipanEngine