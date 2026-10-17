#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "Input/Key.h"
#include "Utilities/ShortTextValues.h"

namespace KashipanEngine::ShortText {
enum class Event { EveryFrame, Start, KeyPressed, KeyHeld, KeyReleased, Timer, ContactEnter, ContactStay, ContactExit, MousePressed, MouseHeld, MouseReleased, ButtonClicked, VariableChanged, ConditionsEntered };
enum class Test { PositionAtMost, PositionAtLeast, VelocityAtMost, VelocityAtLeast, VariableAtMost, VariableAtLeast, DistanceAtMost, DistanceAtLeast, Expression };
enum class Operation { SetVelocity, AddVelocity, SetAcceleration, Translate, Rotate, SetScale, SetPosition, SetRotation, StopVelocity, SetVariable, AddVariable, PlaySound, StopSound, SetVolume, SetVisible, SetActive, CreateObject, CloneObject, DeleteObject, ChangeScene, SetText, ShowVariableText, Follow, MoveToward, TweenPosition, TweenRotation, TweenScale, Wait, EngineCommand, TweenProperty, Parallel, Repeat, Branch, SequenceControl, SetState };
struct Condition { Test test = Test::PositionAtMost; int axis = 1; float value = 0; std::string name; Value data=Value::object(); };
enum class Target { Self, Contact, Name, Tag, Parent, Children, Descendants, ID, LastCreated, Nearest, Range, Index };
inline constexpr std::array<const char *, 12> TargetNames = {"self", "contact", "name", "tag", "parent", "children", "descendants", "id", "last_created", "nearest", "range", "index"};
inline constexpr std::array<const char *, 12> TargetLabels = {"自分", "接触相手", "オブジェクト名", "オブジェクトタグ", "親", "子", "子孫", "ID", "生成結果", "最寄り", "範囲内", "候補番号"};
struct Action {
    Operation operation = Operation::SetVelocity; int axis = 1; float value = 0; std::string name;
    Target target = Target::Self; std::string targetName; std::string reference; std::string text;
    float duration = 1;
    Value data = Value::object();
    std::string targetId; std::string referenceId; float frameSeconds = 0; // runtime only, never serialized
};
struct Rule {
    Event event = Event::EveryFrame;
    Key key = Key::Space;
    float interval = 1;
    std::string tag;
    int button = 0; bool once = false; std::string variable;
    std::vector<Condition> conditions;
    std::vector<Action> actions;
    float cooldown=0; std::string state;float windowStart=0,windowEnd=-1;
};
inline constexpr std::array<const char *, 15> EventNames = {"every_frame", "start", "key_pressed", "key_held", "key_released", "timer", "contact_enter", "contact_stay", "contact_exit", "mouse_pressed", "mouse_held", "mouse_released", "button_clicked", "variable_changed", "conditions_entered"};
inline constexpr std::array<const char *, 9> TestNames = {"position_at_most", "position_at_least", "velocity_at_most", "velocity_at_least", "variable_at_most", "variable_at_least", "distance_at_most", "distance_at_least", "expression"};
inline constexpr std::array<const char *, 35> OperationNames = {"set_velocity", "add_velocity", "set_acceleration", "translate", "rotate", "set_scale", "set_position", "set_rotation", "stop_velocity", "set_variable", "add_variable", "play_sound", "stop_sound", "set_volume", "set_visible", "set_active", "create_object", "clone_object", "delete_object", "change_scene", "set_text", "show_variable_text", "follow", "move_toward", "tween_position", "tween_rotation", "tween_scale", "wait", "engine_command", "tween_property", "parallel", "repeat", "branch", "sequence_control", "set_state"};
inline constexpr std::array<const char *, 15> EventLabels = {"毎フレーム", "開始時", "キーを押した瞬間", "キーを押している間", "キーを離した瞬間", "一定間隔", "接触を開始", "接触している間", "接触を終了", "マウスを押した瞬間", "マウスを押している間", "マウスを離した瞬間", "ボタンクリック", "変数が変化", "条件を満たした瞬間"};
inline constexpr std::array<const char *, 9> TestLabels = {"位置が指定値以下", "位置が指定値以上", "速度が指定値以下", "速度が指定値以上", "シーン変数が指定値以下", "シーン変数が指定値以上", "指定オブジェクトとの距離が以下", "指定オブジェクトとの距離が以上", "式による条件"};
inline constexpr std::array<const char *, 35> OperationLabels = {"速度を代入", "速度を加算", "加速度を代入", "移動（単位/秒）", "回転（度/秒）", "スケールを代入", "位置を代入", "回転角度を代入", "速度をゼロにする", "シーン変数を代入", "シーン変数を加算", "音を再生", "音を停止", "音量を設定", "表示を切り替え", "有効状態を切り替え", "空オブジェクトを生成", "対象を複製", "対象を削除", "シーンへ移動", "テキストを設定", "変数をテキスト表示", "指定対象へ追従", "指定対象へ近づく", "位置を補間", "回転を補間", "スケールを補間", "待機", "拡張部品", "プロパティ補間", "並列実行", "繰り返し", "分岐", "シーケンス制御", "状態変更"};
inline std::string KeyName(Key key) {
    if (key >= Key::A && key <= Key::Z) return std::string(1, static_cast<char>('A' + static_cast<int>(key) - static_cast<int>(Key::A)));
    if (key >= Key::D0 && key <= Key::D9) return std::string(1, static_cast<char>('0' + static_cast<int>(key) - static_cast<int>(Key::D0)));
    for (const auto &[value, name] : std::array<std::pair<Key, const char *>, 10>{{
        {Key::Left, "Left"}, {Key::Right, "Right"}, {Key::Up, "Up"}, {Key::Down, "Down"},
        {Key::Space, "Space"}, {Key::Enter, "Enter"}, {Key::Shift, "Shift"}, {Key::Control, "Control"}, {Key::Escape, "Escape"}, {Key::Tab, "Tab"}}}) {
        if (key == value) return name;
    }
    return {};
}
inline Key ParseKey(const std::string &name) {
    for (int i = static_cast<int>(Key::A); i <= static_cast<int>(Key::F12); ++i) {
        const auto key = static_cast<Key>(i);
        if (!name.empty() && KeyName(key) == name) return key;
    }
    return Key::Unknown;
}
inline bool IsValidName(const std::string &name, bool allowEmpty = false) {
    if ((!allowEmpty && name.empty()) || name.size() > 64) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    return true;
}
inline bool IsContact(Event e) { return e >= Event::ContactEnter && e <= Event::ContactExit; }
inline bool IsVariable(Test t) { return t == Test::VariableAtMost || t == Test::VariableAtLeast; }
inline bool IsVariable(Operation o) { return o == Operation::SetVariable || o == Operation::AddVariable; }
inline int InlineInteger(const Value &v,int minimum,int maximum) {
    if(!v.is_number_integer())throw std::runtime_error("integer required");
    if(v.is_number_unsigned()){if(v.get<uint64_t>()>static_cast<uint64_t>(maximum))throw std::runtime_error("integer range");}
    else {auto value=v.get<int64_t>();if(value<minimum||value>maximum)throw std::runtime_error("integer range");}
    return v.get<int>();
}
inline Action InlineAction(const Value &v) {
    if(!v.is_object())throw std::runtime_error("nested action must be object");Action a;
    auto operation=v.at("operation").get<std::string>();auto found=std::find(OperationNames.begin(),OperationNames.end(),operation);
    if(found==OperationNames.end())throw std::runtime_error("unknown nested operation");a.operation=static_cast<Operation>(found-OperationNames.begin());
    a.axis=InlineInteger(v.value("axis",Value(0)),0,2);a.value=v.value("value",0.0f);a.name=v.value("name",std::string());a.targetName=v.value("targetName",std::string());a.reference=v.value("reference",std::string());a.text=v.value("text",std::string());a.duration=v.value("duration",1.0f);a.data=v.value("data",Value::object());
    auto target=v.value("target",std::string("self"));auto t=std::find(TargetNames.begin(),TargetNames.end(),target);if(t==TargetNames.end())throw std::runtime_error("unknown nested target");a.target=static_cast<Target>(t-TargetNames.begin());return a;
}
inline bool IsValidRule(const Rule &r,int depth=0) {
    if(depth>8)return false;
    if (r.event < Event::EveryFrame || r.event > Event::ConditionsEntered || KeyName(r.key).empty() ||
        !IsValidName(r.tag, true) || !std::isfinite(r.interval) || r.interval < 0.05f || r.interval > 3600 ||
        r.button < 0 || r.button > 2 || !IsValidName(r.variable, true) ||
        r.conditions.size() > 4 || r.actions.empty() || r.actions.size() > 8 || !std::isfinite(r.cooldown)||r.cooldown<0||r.cooldown>3600||!IsValidName(r.state,true)||!std::isfinite(r.windowStart)||r.windowStart<0||r.windowStart>86400||!std::isfinite(r.windowEnd)||(r.windowEnd!=-1&&(r.windowEnd<r.windowStart||r.windowEnd>86400))) return false;
    if (r.event == Event::VariableChanged && !IsValidName(r.variable)) return false;
    if (r.event == Event::ConditionsEntered && r.conditions.empty()) return false;
    for (const auto &c : r.conditions) {
        if (c.test < Test::PositionAtMost || c.test > Test::Expression || c.axis < 0 || c.axis > 2 || !std::isfinite(c.value) || std::abs(c.value) > 10000 || !ValidValue(c.data)) return false;
        if ((IsVariable(c.test) || c.test == Test::DistanceAtMost || c.test == Test::DistanceAtLeast) && !IsValidName(c.name)) return false;
        if ((c.test == Test::DistanceAtMost || c.test == Test::DistanceAtLeast) && c.value < 0) return false;
    }
    for (const auto &a : r.actions) {
        if (a.operation < Operation::SetVelocity || a.operation > Operation::SetState || a.axis < 0 || a.axis > 2 || !std::isfinite(a.value) || std::abs(a.value) > 10000 || !a.data.is_object()||!ValidValue(a.data)) return false;
        if (IsVariable(a.operation) && !IsValidName(a.name)) return false;
        if (!IsValidName(a.name, true)) return false;
        if (a.operation == Operation::SetVolume && (a.value < 0 || a.value > 1)) return false;
        if (a.target < Target::Self || a.target > Target::Index || !IsValidName(a.targetName, true) ||
            !IsValidName(a.reference, true) || a.text.size() > 512 || !std::isfinite(a.duration) || a.duration < 0.01f || a.duration > 3600) return false;
        if ((a.target == Target::Name || a.target == Target::Tag ||a.target==Target::ID) && !IsValidName(a.targetName)) return false;
        if ((a.operation == Operation::CloneObject || a.operation == Operation::ChangeScene || a.operation == Operation::ShowVariableText) && !IsValidName(a.name)) return false;
        if ((a.operation == Operation::Follow || a.operation == Operation::MoveToward) && !IsValidName(a.reference)) return false;
        if (a.operation == Operation::MoveToward && a.value <= 0) return false;
        if ((a.operation == Operation::SetVisible || a.operation == Operation::SetActive) && a.value != 0 && a.value != 1) return false;
        if ((a.operation == Operation::SetScale || a.operation == Operation::TweenScale) && (a.value <= 0 || a.value > 100)) return false;
        if((a.operation==Operation::EngineCommand||a.operation==Operation::TweenProperty||a.operation==Operation::SetState)&&!IsValidName(a.name))return false;
        if(a.operation==Operation::SetState&&a.target!=Target::Self)return false;
        if(a.target==Target::LastCreated&&(a.operation==Operation::Translate||a.operation==Operation::Rotate||a.operation==Operation::AddVelocity))return false;
        try {
            auto validateList=[&](const Value &list){if(!list.is_array()||list.size()>8)return false;for(const auto &v:list){Rule nested;nested.actions={InlineAction(v)};if(!IsValidRule(nested,depth+1))return false;}return true;};
            if(a.operation==Operation::Parallel){auto branches=a.data.at("branches");if(!branches.is_array()||branches.empty()||branches.size()>8)return false;for(auto &b:branches)if(!validateList(b))return false;}
            if(a.operation==Operation::Repeat){InlineInteger(a.data.at("count"),1,32);if(!validateList(a.data.at("actions")))return false;}
            if(a.operation==Operation::Branch){if(!a.data.contains("condition")||!validateList(a.data.at("then"))||(a.data.contains("else")&&!validateList(a.data.at("else"))))return false;}
            if(a.operation==Operation::SequenceControl){InlineInteger(a.data.at("rule"),0,15);auto mode=a.data.at("mode").get<std::string>();if(mode!="cancel"&&mode!="pause"&&mode!="resume"&&mode!="restart"&&mode!="replace")return false;if(mode=="replace"&&!validateList(a.data.at("actions")))return false;}
            if(a.operation==Operation::TweenProperty&&!a.data.contains("to"))return false;
        }catch(...){return false;}
    }
    return true;
}
inline bool NeedsVelocity(const std::vector<Rule> &rules) {
    for (const auto &r : rules) {
        for (const auto &c : r.conditions) if (c.test == Test::VelocityAtMost || c.test == Test::VelocityAtLeast) return true;
        for (const auto &a : r.actions) if (a.target == Target::Self && (a.operation == Operation::SetVelocity || a.operation == Operation::AddVelocity ||
            a.operation == Operation::SetAcceleration || a.operation == Operation::StopVelocity)) return true;
    }
    return false;
}
constexpr size_t KeyCount = static_cast<size_t>(Key::F12) + 1;
struct InputSnapshot {
    std::array<bool, KeyCount> pressed{}, held{}, released{};
    std::array<bool, 3> mousePressed{}, mouseHeld{}, mouseReleased{}; bool buttonClicked = false;
    std::array<bool, 16> contactReady{};
    std::array<std::vector<std::string>, 16> contacts;
};
struct ObjectSnapshot {
    std::string id, name, tag;
    std::array<float, 3> position{}, world{}, rotation{}, scale{1, 1, 1}, velocity{}, acceleration{};
    std::string parent;bool active=true;Value properties=Value::object();std::vector<std::string> components;
};
struct ProgramState {
    std::array<float, 3> position{}, rotation{}, scale{1, 1, 1}, velocity{}, acceleration{};
    std::unordered_map<std::string, float> variables;
    std::unordered_set<std::string> blockedVariables;
    std::unordered_set<std::string> writtenVariables;
    std::vector<Action> effects;
    std::vector<Action> commands;
    std::string selfId; std::array<float, 3> world{}; std::vector<ObjectSnapshot> objects;
    Value environment = Value::object();
    std::string lastCreatedId;std::vector<Action> controls;
};
struct BoundStep { Action action; std::vector<std::string> targets; std::vector<float> starts; bool captured = false; std::vector<Value> propertyStarts;int depth=0; };
struct Sequence { std::vector<BoundStep> steps; size_t step = 0; double elapsed = 0; bool running = false; bool paused=false;std::vector<Sequence> branches;int depth=0;std::string contact; };
struct ProgramMemory {
    bool started = false; std::array<double, 16> timers{}; std::array<std::vector<std::string>, 16> contacts;
    std::array<bool, 16> done{}, seenVariable{}, presentVariable{}, previousCondition{};
    std::array<float, 16> variables{}; std::array<Sequence, 16> sequences;
    Value extensionValues = Value::object();
    std::array<Value,16> typedVariables;std::array<double,16> cooldowns{};std::string state;double elapsed=0;
};
inline ObjectSnapshot *FindObject(ProgramState &s, const std::string &id) {
    for (auto &o : s.objects) if (o.id == id) return &o;
    return nullptr;
}
inline std::string UniqueName(const ProgramState &s, const std::string &name) {
    std::string result;
    for (const auto &o : s.objects) if (o.name == name) { if (!result.empty()) return {}; result = o.id; }
    return result;
}
inline bool ConditionsPass(const Rule &r, const ProgramState &s) {
    for (const auto &c : r.conditions) {
        const auto axis = static_cast<size_t>(c.axis);
        bool pass = false;
        switch (c.test) {
        case Test::PositionAtMost: pass = s.position[axis] <= c.value; break;
        case Test::PositionAtLeast: pass = s.position[axis] >= c.value; break;
        case Test::VelocityAtMost: pass = s.velocity[axis] <= c.value; break;
        case Test::VelocityAtLeast: pass = s.velocity[axis] >= c.value; break;
        case Test::VariableAtMost: case Test::VariableAtLeast: {
            const auto found = s.variables.find(c.name);
            pass = found != s.variables.end() && std::isfinite(found->second) &&
                (c.test == Test::VariableAtMost ? found->second <= c.value : found->second >= c.value); break;
        }
        case Test::DistanceAtMost: case Test::DistanceAtLeast: {
            const auto id = UniqueName(s, c.name);
            for (const auto &o : s.objects) if (!id.empty() && o.id == id) {
                double distance = 0; for (size_t k = 0; k < 3; ++k) distance += std::pow(o.world[k] - s.world[k], 2);
                distance = std::sqrt(distance);
                pass = c.test == Test::DistanceAtMost ? distance <= c.value : distance >= c.value;
            }
            break;
        }
        case Test::Expression: try {auto value=Evaluate(c.data,s.environment);pass=value.is_boolean()&&value.get<bool>();}catch(...){pass=false;}break;
        }
        if (!pass) return false;
    }
    return true;
}
inline std::vector<std::string> BindTargets(const Action &a, const ProgramState &s, const std::string &contact) {
    if (a.target == Target::Self) return {s.selfId};
    if (a.target == Target::Contact) return contact.empty() ? std::vector<std::string>{} : std::vector<std::string>{contact};
    if (a.target == Target::Name) { const auto id = UniqueName(s, a.targetName); return id.empty() ? std::vector<std::string>{} : std::vector<std::string>{id}; }
    if(a.target==Target::ID){for(auto &o:s.objects)if(o.id==a.targetName)return {o.id};return {};}
    if(a.target==Target::LastCreated)return {"@last_created"};
    std::vector<std::string> result;
    if(a.target==Target::Parent){for(auto &o:s.objects)if(o.id==s.selfId&&!o.parent.empty())return {o.parent};return {};}
    if(a.target==Target::Children||a.target==Target::Descendants){
        std::unordered_set<std::string> ancestry{s.selfId};
        for(size_t pass=0;pass<(a.target==Target::Children?1:32);++pass){bool added=false;for(auto &o:s.objects)if(!ancestry.contains(o.id)&&ancestry.contains(o.parent)&&result.size()<32){result.push_back(o.id);if(a.target==Target::Descendants)ancestry.insert(o.id);added=true;}if(!added)break;}
        return result;
    }
    for (const auto &o : s.objects) if ((a.target==Target::Tag?o.tag==a.targetName:((a.targetName.empty()||o.tag==a.targetName)&&o.active&&o.id!=s.selfId))&&(a.target!=Target::Tag||result.size()<32)) result.push_back(o.id);
    if(a.target==Target::Nearest||a.target==Target::Range){
        std::unordered_map<std::string,double> distances;for(auto &o:s.objects){double d=0;for(size_t k=0;k<3;++k)d+=std::pow(o.world[k]-s.world[k],2);distances[o.id]=d;}
        auto distance=[&](const std::string &id){return distances.at(id);};
        std::stable_sort(result.begin(),result.end(),[&](auto &x,auto &y){return distance(x)<distance(y);});
        if(a.target==Target::Nearest){if(result.size()>1)result.resize(1);}else {try{double radius=a.data.value("radius",static_cast<double>(a.value));if(radius<0)return {};std::erase_if(result,[&](auto &id){return distance(id)>radius*radius;});}catch(...){return {};}}
    }
    if(a.target==Target::Index){try{int index=a.data.value("index",0);return index>=0&&static_cast<size_t>(index)<result.size()?std::vector<std::string>{result[index]}:std::vector<std::string>{};}catch(...){return {};}}
    if(result.size()>32)result.resize(32);
    return result;
}
inline void ExecuteAction(Action a, const std::string &id, ProgramState &s, double dt) {
    if (s.commands.size() >= 512) return;
    const bool self = id == s.selfId;
    auto *object = self ? nullptr : FindObject(s, id);
    if(id=="@last_created"){a.targetId=id;a.frameSeconds=static_cast<float>(dt);s.commands.push_back(std::move(a));return;}
    if (!self && !object) return;
    auto &position = self ? s.position : object->position;
    auto &rotation = self ? s.rotation : object->rotation;
    auto &scale = self ? s.scale : object->scale;
    auto &velocity = self ? s.velocity : object->velocity;
    auto &acceleration = self ? s.acceleration : object->acceleration;
    const auto axis = static_cast<size_t>(a.axis);
    constexpr float radians = 0.017453292519943295f;
    a.targetId = id; a.frameSeconds = static_cast<float>(dt);
    switch (a.operation) {
    case Operation::SetVelocity: velocity[axis] = a.value; break;
    case Operation::AddVelocity: velocity[axis] += a.value; a.operation = Operation::SetVelocity; a.value = velocity[axis]; break;
    case Operation::SetAcceleration: acceleration[axis] = a.value; break;
    case Operation::Translate: position[axis] += a.value * static_cast<float>(dt); a.operation = Operation::SetPosition; a.value = position[axis]; break;
    case Operation::Rotate: rotation[axis] += a.value * radians * static_cast<float>(dt); a.operation = Operation::SetRotation; a.value = rotation[axis] / radians; break;
    case Operation::SetScale: scale[axis] = a.value; break;
    case Operation::SetPosition: position[axis] = a.value; break;
    case Operation::SetRotation: rotation[axis] = a.value * radians; break;
    case Operation::StopVelocity: velocity[axis] = 0; break;
    case Operation::SetVariable: case Operation::AddVariable:
        if (s.blockedVariables.contains(a.name)) return;
        { auto &value = s.variables[a.name]; value = a.operation == Operation::SetVariable ? a.value : std::clamp(value + a.value, -1000000000.0f, 1000000000.0f);
          s.writtenVariables.insert(a.name); a.operation = Operation::SetVariable; a.value = value; }
        break;
    case Operation::PlaySound: case Operation::StopSound: case Operation::SetVolume: s.effects.push_back(a); break;
    case Operation::SetVisible: case Operation::SetActive: case Operation::CreateObject: case Operation::CloneObject: case Operation::DeleteObject:
    case Operation::ChangeScene: case Operation::SetText: case Operation::ShowVariableText: case Operation::Follow: case Operation::MoveToward: break;
    case Operation::EngineCommand: break;
    case Operation::SequenceControl: s.controls.push_back(a);return;
    case Operation::SetState: s.environment["state"]=a.name;return;
    case Operation::TweenPosition: case Operation::TweenRotation: case Operation::TweenScale: case Operation::Wait:
    case Operation::TweenProperty:case Operation::Parallel:case Operation::Repeat:case Operation::Branch:return;
    }
    s.commands.push_back(std::move(a));
}
inline void AppendSteps(Sequence &q,const Value &actions,const ProgramState &s,const std::string &contact) {
    if(!actions.is_array()||actions.size()>8||q.steps.size()+actions.size()>256)throw std::runtime_error("sequence action limit");
    for(const auto &value:actions){auto action=InlineAction(value);BoundStep step;step.depth=q.depth;step.action=action;step.targets=BindTargets(action,s,contact);step.action.referenceId=UniqueName(s,action.reference);if(action.operation==Operation::Wait)step.targets={s.selfId};q.steps.push_back(std::move(step));}
}
inline float Eased(float t,const Value &data) {
    auto name=data.value("easing",std::string("linear"));
    if(name=="ease_in")return t*t;if(name=="ease_out")return 1-(1-t)*(1-t);if(name=="ease_in_out"||name=="smoothstep")return t*t*(3-2*t);
    if(name=="curve") {auto curve=data.at("curve");if(!curve.is_array()||curve.size()<2||curve.size()>64)throw std::runtime_error("curve needs 2..64 samples");float index=t*static_cast<float>(curve.size()-1);size_t a=std::min(static_cast<size_t>(index),curve.size()-2);float p=curve[a].get<float>(),n=curve[a+1].get<float>();return p+(n-p)*(index-static_cast<float>(a));}
    if(name!="linear")throw std::runtime_error("unknown easing");return t;
}
inline Value InterpolateValue(const Value &from,const Value &to,float t) {
    if(from.is_number()&&to.is_number()){double v=from.get<double>()+(to.get<double>()-from.get<double>())*t;if(!std::isfinite(v)||std::abs(v)>1e9)throw std::runtime_error("interpolation range");return v;}
    if(from.is_array()&&to.is_array()&&from.size()==to.size()&&from.size()<=64){Value result=Value::array();for(size_t i=0;i<from.size();++i)result.push_back(InterpolateValue(from[i],to[i],t));return result;}throw std::runtime_error("interpolation requires matching numeric values");
}
inline double AdvanceSequence(Sequence &q, ProgramState &s, double dt) {
    if(q.paused)return dt;
    double remaining = dt;
    size_t budget=0;
    while (q.running && q.step < q.steps.size()&&++budget<=512) {
        auto &step = q.steps[q.step];
        const auto op = step.action.operation;
        if(op==Operation::Repeat||op==Operation::Branch) {
            if(step.depth>=8){q.running=false;return remaining;}
            try {Sequence expansion;expansion.depth=step.depth+1;
                if(op==Operation::Repeat){int count=step.action.data.at("count").get<int>();for(int i=0;i<count;++i)AppendSteps(expansion,step.action.data.at("actions"),s,q.contact);}
                else {auto value=Evaluate(step.action.data.at("condition"),s.environment);if(!value.is_boolean())throw std::runtime_error("branch condition must be boolean");auto key=value.get<bool>()?"then":"else";if(step.action.data.contains(key))AppendSteps(expansion,step.action.data.at(key),s,q.contact);}
                if(q.steps.size()+expansion.steps.size()>256)throw std::runtime_error("expanded sequence limit");q.steps.erase(q.steps.begin()+q.step);q.steps.insert(q.steps.begin()+q.step,expansion.steps.begin(),expansion.steps.end());
            }catch(...){q.running=false;return remaining;}continue;
        }
        if(op==Operation::Parallel) {
            try {if(step.depth>=8)throw std::runtime_error("parallel depth limit");if(!step.captured){step.captured=true;for(const auto &branch:step.action.data.at("branches")){Sequence child;child.running=true;child.depth=step.depth+1;child.contact=q.contact;AppendSteps(child,branch,s,q.contact);q.branches.push_back(std::move(child));}}
                double used=0;bool running=false;for(auto &child:q.branches){double unused=AdvanceSequence(child,s,remaining);used=std::max(used,remaining-unused);running|=child.running;}remaining-=used;if(running)return 0;q.branches.clear();
            }catch(...){q.running=false;return remaining;}
            ++q.step;q.elapsed=0;continue;
        }
        const bool property=op==Operation::TweenProperty;
        const bool tween = op == Operation::TweenPosition || op == Operation::TweenRotation || op == Operation::TweenScale ||property;
        if (op == Operation::Wait || tween) {
            try {if (!step.captured) {
                step.captured = true;
                for (const auto &id : step.targets) {
                    const bool self = id == s.selfId; const auto *o = self ? nullptr : FindObject(s, id);
                    const auto axis = static_cast<size_t>(step.action.axis);
                    if(property) {Value start;auto *object=FindObject(s,id);if(step.action.data.contains("from"))start=Evaluate(step.action.data.at("from"),s.environment);else if(object&&object->properties.contains(step.action.name))start=object->properties.at(step.action.name);step.propertyStarts.push_back(start);continue;}
                    float start = 0;
                    if (self || o) start = op == Operation::TweenPosition ? (self ? s.position[axis] : o->position[axis]) :
                        op == Operation::TweenRotation ? (self ? s.rotation[axis] : o->rotation[axis]) / 0.017453292519943295f : (self ? s.scale[axis] : o->scale[axis]);
                    if(step.action.data.contains("from"))start=step.action.data.at("from").get<float>();step.starts.push_back(start);
                }
            }}catch(...){q.running=false;return remaining;}
            const double consumed = std::min(remaining, static_cast<double>(step.action.duration) - q.elapsed);
            q.elapsed += consumed; remaining -= consumed;
            if (tween) for (size_t k = 0; k < step.targets.size(); ++k) {
                Action a = step.action;
                float t=0;try{t=Eased(static_cast<float>(std::min(1.0,q.elapsed/a.duration)),a.data);}catch(...){q.running=false;return remaining;}
                if(property){try{auto value=InterpolateValue(step.propertyStarts[k],Evaluate(a.data.at("to"),s.environment),t);a.operation=Operation::EngineCommand;a.data={{"path",a.name},{"value",value}};a.name="property.set";if(auto *object=FindObject(s,step.targets[k]))object->properties[step.action.name]=value;ExecuteAction(a,step.targets[k],s,consumed);}catch(...){q.running=false;return remaining;}continue;}
                float difference=a.value-step.starts[k];if(op==Operation::TweenRotation&&a.data.value("shortest",false))difference=std::remainder(difference,360.0f);
                a.value = step.starts[k] + difference * t;
                a.operation = op == Operation::TweenPosition ? Operation::SetPosition : op == Operation::TweenRotation ? Operation::SetRotation : Operation::SetScale;
                ExecuteAction(a, step.targets[k], s, consumed);
            }
            if (q.elapsed + 1e-7 < step.action.duration) return 0;
        } else for (const auto &id : step.targets) ExecuteAction(step.action, id, s, remaining);
        ++q.step; q.elapsed = 0;
    }
    if (q.step == q.steps.size()) q.running = false;
    return remaining;
}
inline void TickProgram(const std::vector<Rule> &rules, ProgramMemory &memory, ProgramState &state, const InputSnapshot &input, double dt) {
    state.effects.clear(); state.commands.clear(); state.writtenVariables.clear();state.controls.clear();
    if (rules.empty() || rules.size() > 16 || !std::isfinite(dt) || dt < 0 || dt > 1) return;
    for (const auto &r : rules) if (!IsValidRule(r)) return;
    state.environment["state"]=memory.state;
    memory.elapsed+=dt;state.environment["elapsed"]=memory.elapsed;
    for(auto &cooldown:memory.cooldowns)cooldown=std::max(0.0,cooldown-dt);
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto &r = rules[i]; const auto key = static_cast<size_t>(r.key);
        if(!r.state.empty()&&r.state!=state.environment.value("state",std::string())){memory.sequences[i]={};continue;}
        const bool inWindow=memory.elapsed>=r.windowStart&&(r.windowEnd<0||memory.elapsed<=r.windowEnd);
        bool fire = false; std::vector<std::string> partners;
        switch (r.event) {
        case Event::EveryFrame: fire = true; break;
        case Event::Start: fire = !memory.started; break;
        case Event::KeyPressed: fire = input.pressed[key]; break;
        case Event::KeyHeld: fire = input.held[key]; break;
        case Event::KeyReleased: fire = input.released[key]; break;
        case Event::Timer: memory.timers[i] += dt; if (memory.timers[i] >= r.interval) { fire = true; memory.timers[i] = std::fmod(memory.timers[i], r.interval); } break;
        case Event::ContactEnter: case Event::ContactStay: case Event::ContactExit: {
            if (!input.contactReady[i]) break;
            auto current = input.contacts[i]; std::sort(current.begin(), current.end()); current.erase(std::unique(current.begin(), current.end()), current.end());
            if (current.size() > 32) current.resize(32);
            const auto &previous = memory.contacts[i];
            if (r.event == Event::ContactStay) partners = current;
            else { const auto &from = r.event == Event::ContactEnter ? current : previous; const auto &against = r.event == Event::ContactEnter ? previous : current;
                for (const auto &id : from) if (!std::binary_search(against.begin(), against.end(), id)) partners.push_back(id); }
            memory.contacts[i] = std::move(current); fire = !partners.empty(); break;
        }
        case Event::MousePressed: fire = input.mousePressed[static_cast<size_t>(r.button)]; break;
        case Event::MouseHeld: fire = input.mouseHeld[static_cast<size_t>(r.button)]; break;
        case Event::MouseReleased: fire = input.mouseReleased[static_cast<size_t>(r.button)]; break;
        case Event::ButtonClicked: fire = memory.started && input.buttonClicked; break;
        case Event::VariableChanged: {
            Value value;try{value=Evaluate(Value{{"read",r.variable}},state.environment);if(value.is_null()&&r.variable.find(':')==std::string::npos)value=Evaluate(Value{{"read","scene:"+r.variable}},state.environment);}catch(...){value=nullptr;}
            const auto found = state.variables.find(r.variable);if(value.is_null()&&found!=state.variables.end())value=found->second;const bool present=!value.is_null();
            fire = memory.seenVariable[i] && (present != memory.presentVariable[i] || (present && value != memory.typedVariables[i]));
            memory.seenVariable[i] = true; memory.presentVariable[i] = present;memory.typedVariables[i]=value;if(value.is_number())memory.variables[i]=value.get<float>();break;
        }
        case Event::ConditionsEntered: { const bool now = ConditionsPass(r, state); fire = now && !memory.previousCondition[i]; memory.previousCondition[i] = now; break; }
        }
        auto &sequence = memory.sequences[i];
        if (sequence.running) { AdvanceSequence(sequence, state, dt); continue; }
        if (!fire || !inWindow || (r.once && memory.done[i])||memory.cooldowns[i]>1e-7) continue;
        if (partners.empty()) partners.push_back({});
        for (const auto &partner : partners) {
            if (!ConditionsPass(r, state) || (r.once && memory.done[i])) continue;
            sequence = {}; sequence.running = true;sequence.contact=partner;
            for (const auto &action : r.actions) {
                BoundStep step; step.action = action; step.targets = BindTargets(action, state, partner);
                step.action.referenceId = UniqueName(state, action.reference);
                if (action.operation == Operation::Wait) step.targets = {state.selfId};
                sequence.steps.push_back(std::move(step));
            }
            memory.done[i] = true;memory.cooldowns[i]=r.cooldown;AdvanceSequence(sequence, state, dt);
            if (sequence.running) break; // one active sequence per rule, no unbounded event backlog
        }
    }
    for(const auto &control:state.controls)try {
        const size_t index=control.data.at("rule").get<size_t>();if(index>=rules.size())continue;auto &sequence=memory.sequences[index];auto mode=control.data.at("mode").get<std::string>();
        if(mode=="cancel")sequence={};else if(mode=="pause")sequence.paused=true;else if(mode=="resume")sequence.paused=false;
        else if(mode=="restart"||mode=="replace") {sequence={};sequence.running=true;memory.done[index]=false;
            if(mode=="replace")AppendSteps(sequence,control.data.at("actions"),state,{});else for(auto &action:rules[index].actions){BoundStep step;step.action=action;step.targets=BindTargets(action,state,{});step.action.referenceId=UniqueName(state,action.reference);if(action.operation==Operation::Wait)step.targets={state.selfId};sequence.steps.push_back(std::move(step));}
        }
    }catch(...){/* invalid controls are refused by validation */}
    memory.state=state.environment.value("state",std::string());
    memory.started = true;
}
} // namespace KashipanEngine::ShortText
