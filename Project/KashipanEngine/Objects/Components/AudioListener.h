#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class AudioListener final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(AudioListener, 1, )
    COMPONENT_CATEGORY("Audio")
    ~AudioListener() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override { return std::make_unique<AudioListener>(); }
};

REGISTER_COMPONENT_OBJECT(AudioListener)

} // namespace KashipanEngine
