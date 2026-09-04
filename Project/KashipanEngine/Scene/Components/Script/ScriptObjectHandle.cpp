#include "Scene/Components/Script/ScriptObjectHandle.h"

#include <angelscript.h>

#include "Debug/Logger.h"
#include "Objects/EmptyObject.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

EmptyObject *ScriptObjectHandle::Resolve() const {
    LogScope scope;
    SceneContext *sceneContext = ScriptExecutionScope::GetCurrentSceneContext();
    return sceneContext ? sceneContext->GetSceneObject(id_) : nullptr;
}

ScriptObjectHandle *ScriptObjectHandle::Create(EmptyObject *obj) {
    LogScope scope;
    if (!obj) return nullptr;
    return new ScriptObjectHandle(obj->GetObjectID());
}

ScriptObjectHandle *ScriptObjectHandle::CreateFromID(const UUID128 &id) {
    LogScope scope;
    if (!id.IsValid()) return nullptr;
    return new ScriptObjectHandle(id);
}

void ThrowDestroyedObjectException() {
    LogScope scope;
    if (asIScriptContext *context = asGetActiveContext()) {
        context->SetException("参照先のオブジェクトは既に削除されています");
    }
}

} // namespace KashipanEngine
