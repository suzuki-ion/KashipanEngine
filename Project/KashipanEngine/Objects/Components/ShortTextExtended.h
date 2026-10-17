#pragma once
#include "Utilities/ShortTextBehavior.h"

namespace KashipanEngine {
class EmptyObject;
class SceneContext;
namespace ShortText {
// Domain adapters share stable scene references and report unsupported commands explicitly.
struct CommandContext {
    const EmptyObject *owner = nullptr;
    EmptyObject *target = nullptr;
    SceneContext *scene = nullptr;
    ProgramMemory *memory = nullptr;
    double dt = 0;
    Value *environment = nullptr;
};
bool ExecuteRenderingCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error);
bool ExecutePhysicsCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error);
bool ExecuteMediaCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error);
bool ExecuteSpatialCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error);
bool ReadExtendedProperty(const EmptyObject *object, const std::string &path, Value &value);
bool WriteExtendedProperty(EmptyObject *object, const std::string &path, const Value &value);
void ReadExtendedValues(const EmptyObject *owner, SceneContext *scene, ProgramState &state, ProgramMemory *memory = nullptr, double dt = 0);
bool ApplyExtendedCommand(const Action &action, CommandContext &context, std::string &error);
void ReadPhysicsValues(const EmptyObject *owner, SceneContext *scene, ProgramState &state, ProgramMemory *memory, double dt);
void ReadMediaValues(const EmptyObject *owner, SceneContext *scene, ProgramState &state, ProgramMemory *memory, double dt);
}
} // namespace KashipanEngine
