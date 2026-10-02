"""Compile focused regression cases against declarations/methods extracted from production.

Run from a Visual Studio developer prompt: python Project/Tests/EncapsulationRegression.py
TypeInfo and scene/context dependencies are stubbed; graphics and scene runtime are not tested.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / "KashipanEngine"


def section(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]


def method(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]


header = (ROOT / "Objects/IObjectComponent.h").read_text(encoding="utf-8-sig")
commands = (ROOT / "Scene/Editor/SceneEditorCommands.h").read_text(encoding="utf-8-sig")
source = (ROOT / "Scene/Editor/SceneEditorCommands.cpp").read_text(encoding="utf-8-sig")
passkeys = (ROOT / "Utilities/Passkeys.h").read_text(encoding="utf-8-sig")
scene_context = (ROOT / "Scene/SceneContext.h").read_text(encoding="utf-8-sig")

pool_code = "#include <cassert>\n#include <cstddef>\n#include <type_traits>\n"
pool_code += section(passkeys, "template <typename T>\nclass Passkey", "/// @brief WinMain")
pool_code += """
class EmptyObject;
class SceneContext;
struct IComponentPoolBase {};
class Scene {
    friend class SceneContext;
    IComponentPoolBase pool_;
    IComponentPoolBase *GetOrCreateComponentPool(size_t) { return &pool_; }
};
class SceneContext {
    Scene *owner_;
public:
    explicit SceneContext(Scene *owner) : owner_(owner) {}
"""
pool_code += method(scene_context, "IComponentPoolBase *GetOrCreateComponentPool")
pool_code += """
};
class EmptyObject {
public:
    static IComponentPoolBase *Access(SceneContext &context) {
        return context.GetOrCreateComponentPool(Passkey<EmptyObject>{}, 0);
    }
};
template<class T> concept UnrestrictedPool = requires(T &context) {
    context.GetOrCreateComponentPool(size_t{});
};
static_assert(!UnrestrictedPool<SceneContext>);
static_assert(!std::is_default_constructible_v<Passkey<EmptyObject>>);
int main() {
    Scene scene;
    SceneContext context(&scene);
    assert(EmptyObject::Access(context));
}
"""

code = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
struct TypeInfo {};
template<class T> TypeInfo GetValueType() { return {}; }
struct SceneEditorContext {};
struct SceneEditor {};
template<class T> struct Passkey {};
class IObjectComponent {
public:
'''
code += section(header, "    class MemberVariable {", "    /// @brief コンポーネントの型IDを取得")
code += section(header, "    const MemberVariable *GetMemberVariable", "\nprotected:")
code += "\nprotected:\n"
code += section(header, "    template <typename T>\n    void AddMemberVariable", "#define ADD_MEMBER_VARIABLE")
code += "\nprivate:\n    std::unordered_map<std::string, MemberVariable> memberVariables_;\n};\n"
code += section(commands, "class IEditorCommand {", "//==================================================")
code += section(commands, "class CompositeCommand final", "//==================================================")
code += section(commands, "class SceneEditorCommands final", "} // namespace")
for signature in [
    "bool CompositeCommand::Execute", "bool CompositeCommand::Undo",
    "bool CompositeCommand::HasAppliedChanges", "bool SceneEditorCommands::Execute",
    "void SceneEditorCommands::PushExecuted", "bool SceneEditorCommands::Undo",
    "bool SceneEditorCommands::Redo", "void SceneEditorCommands::PushToUndoStack",
]:
    code += method(source, signature) + "\n"

code += r'''
struct Probe : IObjectComponent {
    float value = 1;
    int notifications = 0;
    int checked = 2;
    const int derived = 3;
    Probe() {
        AddMemberVariable("value", &value, [this] { ++notifications; });
        AddReadOnlyMemberVariable("derived", &derived);
        AddMemberProperty<int>("checked", &checked, [this](const int &v) {
            if (v < 0) return false;
            checked = v;
            return true;
        });
    }
};
template<class T> concept ExposesPointer = requires(T t) { t.ptr; };
template<class T> concept ExposesStorage = requires(T t) { t.value_; };
template<class T> concept ExposesNotification = requires(T t) { t.onModified; };
template<class T> concept ExposesWriter = requires(T t) { t.write_; };
static_assert(!ExposesPointer<IObjectComponent::MemberVariable>);
static_assert(!ExposesStorage<IObjectComponent::MemberVariable>);
static_assert(!ExposesNotification<IObjectComponent::MemberVariable>);
static_assert(!ExposesWriter<IObjectComponent::MemberVariable>);
struct Toggle : IEditorCommand {
    int &value;
    bool failExecute = false;
    bool failAfterChange = false;
    bool failUndo = false;
    int executeCalls = 0;
    int undoCalls = 0;
    explicit Toggle(int &v) : value(v) {}
    bool Execute(SceneEditorContext *) override {
        ++executeCalls;
        SetAppliedChangesAfterFailure(false);
        if (failExecute) return false;
        ++value;
        if (failAfterChange) {
            SetAppliedChangesAfterFailure(true);
            return false;
        }
        return true;
    }
    bool Undo(SceneEditorContext *) override {
        ++undoCalls;
        if (failUndo) return false;
        --value;
        return true;
    }
    std::string GetName() const override { return "toggle"; }
};
int main() {
    Probe p;
    float copy = 0;
    assert(p.GetMemberValue("value", copy) && copy == 1);
    copy = 9;
    assert(p.value == 1);
    assert(p.SetMemberValue("value", 4.0f) && p.value == 4 && p.notifications == 1);
    assert(!p.SetMemberValue("value", 4) && p.notifications == 1);
    assert(!p.SetMemberValue("derived", 9) && p.derived == 3);
    assert(!p.GetMemberVariable("derived")->IsWritable());
    int wrongType = 0;
    assert(!p.GetMemberValue("value", wrongType));
    assert(!p.SetMemberValue("missing", 1));
    assert(!p.SetMemberValue("checked", -1) && p.checked == 2);
    assert(p.SetMemberValue("checked", 7) && p.checked == 7);

    SceneEditorContext context;
    SceneEditorCommands history(Passkey<SceneEditor>{}, &context);
    int first = 0, second = 0;
    auto group = std::make_unique<CompositeCommand>("group");
    auto a = std::make_unique<Toggle>(first);
    auto b = std::make_unique<Toggle>(second);
    b->failExecute = true;
    group->AddCommand(std::move(a));
    group->AddCommand(std::move(b));
    assert(!history.Execute(std::move(group)));
    assert(first == 0 && second == 0 && !history.CanUndo());

    group = std::make_unique<CompositeCommand>("recovery");
    a = std::make_unique<Toggle>(first);
    b = std::make_unique<Toggle>(second);
    Toggle *aPtr = a.get(), *bPtr = b.get();
    a->failUndo = true;
    b->failExecute = true;
    group->AddCommand(std::move(a));
    group->AddCommand(std::move(b));
    assert(!history.Execute(std::move(group)));
    assert(first == 1 && history.CanUndo() && !history.CanRedo());
    assert(!history.Undo() && history.CanUndo() && !history.CanRedo());
    aPtr->failUndo = false;
    assert(history.Undo() && first == 0 && history.CanRedo());
    // Failed redo with successful rollback must remain on the redo stack.
    assert(!history.Redo() && first == 0 && history.CanRedo() && !history.CanUndo());
    bPtr->failExecute = false;
    assert(history.Redo() && first == 1 && second == 1);
    // Partial undo: do not undo a successful child twice on retry.
    aPtr->failUndo = true;
    assert(!history.Undo() && first == 1 && second == 0);
    int bUndos = bPtr->undoCalls;
    aPtr->failUndo = false;
    assert(history.Undo() && first == 0 && bPtr->undoCalls == bUndos);
    // Failed redo rollback retains a recovery command in undo history.
    aPtr->failUndo = true;
    bPtr->failExecute = true;
    assert(!history.Redo() && history.CanUndo() && !history.CanRedo() && first == 1);
    aPtr->failUndo = false;
    assert(history.Undo() && first == 0);
    history.Clear();
    // Already applied composites (PushExecuted) and nested composites.
    first = 1;
    group = std::make_unique<CompositeCommand>("already applied");
    group->AddCommand(std::make_unique<Toggle>(first));
    history.PushExecuted(std::move(group));
    assert(history.Undo() && first == 0);
    history.Clear();
    group = std::make_unique<CompositeCommand>("outer");
    auto inner = std::make_unique<CompositeCommand>("inner");
    inner->AddCommand(std::make_unique<Toggle>(first));
    b = std::make_unique<Toggle>(second);
    b->failExecute = true;
    inner->AddCommand(std::move(b));
    group->AddCommand(std::move(inner));
    assert(!history.Execute(std::move(group)) && first == 0 && !history.CanUndo());
    // The failing child itself can report a change that still needs rollback.
    group = std::make_unique<CompositeCommand>("failed child with changes");
    a = std::make_unique<Toggle>(first);
    a->failAfterChange = true;
    group->AddCommand(std::move(a));
    assert(!history.Execute(std::move(group)) && first == 0 && !history.CanUndo());
    a = std::make_unique<Toggle>(first);
    aPtr = a.get();
    a->failAfterChange = true;
    a->failUndo = true;
    assert(!history.Execute(std::move(a)) && first == 1 && history.CanUndo());
    assert(!history.Undo() && history.CanUndo());
    aPtr->failUndo = false;
    assert(history.Undo() && first == 0);
}
'''

with tempfile.TemporaryDirectory(prefix="kashipan-regression-") as directory:
    for name, contents in [("regression", code), ("pool_access", pool_code)]:
        cpp = Path(directory) / f"{name}.cpp"
        exe = Path(directory) / f"{name}.exe"
        cpp.write_text(contents, encoding="utf-8")
        subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8", str(cpp),
                        f"/Fe:{exe}", f"/Fo:{Path(directory) / (name + '.obj')}"], check=True)
        subprocess.run([str(exe)], check=True)
print("Pool access, member access and Command failure/retry regression cases passed.")
