#pragma once
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Assets/TextureManager.h"

namespace KashipanEngine {

struct Material {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // RGBA
    TextureManager::TextureHandle textureHandle = TextureManager::kInvalidHandle;
};

} // namespace KashipanEngine