#include "Scene/Components/Script/ScriptObjectHandle.h"

#include <angelscript.h>

#include "Objects/EmptyObject.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

EmptyObject *ScriptObjectHandle::Resolve() const {
    SceneContext *sceneContext = ScriptExecutionScope::GetCurrentSceneContext();
    return sceneContext ? sceneContext->GetSceneObject(id_) : nullptr;
}

ScriptObjectHandle *ScriptObjectHandle::Create(EmptyObject *obj) {
    if (!obj) return nullptr;
    return new ScriptObjectHandle(obj->GetObjectID());
}

ScriptObjectHandle *ScriptObjectHandle::CreateFromID(const UUID128 &id) {
    if (!id.IsValid()) return nullptr;
    return new ScriptObjectHandle(id);
}

void ThrowDestroyedObjectException() {
    if (asIScriptContext *context = asGetActiveContext()) {
        context->SetException("参照先のオブジェクトは既に削除されています");
    }
}

} // namespace KashipanEngine
