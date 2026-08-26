#include "Debug/ScriptDebugDraw.h"

namespace KashipanEngine {

std::vector<ScriptDebugDraw::Line> ScriptDebugDraw::lines_;

void ScriptDebugDraw::AddLine(const Vector3 &start, const Vector3 &end, const Vector4 &color) {
    lines_.push_back(Line{ start, end, color });
}

const std::vector<ScriptDebugDraw::Line> &ScriptDebugDraw::GetLines() {
    return lines_;
}

void ScriptDebugDraw::Clear() {
    lines_.clear();
}

} // namespace KashipanEngine
