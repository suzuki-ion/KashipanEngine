#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include "Input/Key.h"

namespace KashipanEngine::ShortText {
enum class Event { EveryFrame, Start, KeyPressed, KeyHeld, KeyReleased, Timer };
enum class Test { PositionAtMost, PositionAtLeast, VelocityAtMost, VelocityAtLeast };
enum class Operation { SetVelocity, AddVelocity, SetAcceleration, Translate, Rotate, SetScale, SetPosition, SetRotation, StopVelocity };
struct Condition { Test test = Test::PositionAtMost; int axis = 1; float value = 0; };
struct Action { Operation operation = Operation::SetVelocity; int axis = 1; float value = 0; };
struct Rule {
    Event event = Event::EveryFrame;
    Key key = Key::Space;
    float interval = 1;
    std::vector<Condition> conditions;
    std::vector<Action> actions;
};
inline constexpr std::array<const char *, 6> EventNames = {"every_frame", "start", "key_pressed", "key_held", "key_released", "timer"};
inline constexpr std::array<const char *, 4> TestNames = {"position_at_most", "position_at_least", "velocity_at_most", "velocity_at_least"};
inline constexpr std::array<const char *, 9> OperationNames = {"set_velocity", "add_velocity", "set_acceleration", "translate", "rotate", "set_scale", "set_position", "set_rotation", "stop_velocity"};
inline constexpr std::array<const char *, 6> EventLabels = {"毎フレーム", "開始時", "キーを押した瞬間", "キーを押している間", "キーを離した瞬間", "一定間隔"};
inline constexpr std::array<const char *, 4> TestLabels = {"位置が指定値以下", "位置が指定値以上", "速度が指定値以下", "速度が指定値以上"};
inline constexpr std::array<const char *, 9> OperationLabels = {"速度を代入", "速度を加算", "加速度を代入", "移動（単位/秒）", "回転（度/秒）", "スケールを代入", "位置を代入", "回転角度を代入", "速度をゼロにする"};
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
inline bool IsValidRule(const Rule &r) {
    if (r.event < Event::EveryFrame || r.event > Event::Timer || KeyName(r.key).empty() ||
        !std::isfinite(r.interval) || r.interval < 0.05f || r.interval > 3600 ||
        r.conditions.size() > 4 || r.actions.empty() || r.actions.size() > 8) return false;
    for (const auto &c : r.conditions) {
        if (c.test < Test::PositionAtMost || c.test > Test::VelocityAtLeast || c.axis < 0 || c.axis > 2 || !std::isfinite(c.value) || std::abs(c.value) > 10000) return false;
    }
    for (const auto &a : r.actions) {
        if (a.operation < Operation::SetVelocity || a.operation > Operation::StopVelocity || a.axis < 0 || a.axis > 2 || !std::isfinite(a.value) || std::abs(a.value) > 10000) return false;
        if (a.operation == Operation::SetScale && (a.value <= 0 || a.value > 100)) return false;
    }
    return true;
}
inline bool NeedsVelocity(const std::vector<Rule> &rules) {
    for (const auto &r : rules) {
        for (const auto &c : r.conditions) if (c.test == Test::VelocityAtMost || c.test == Test::VelocityAtLeast) return true;
        for (const auto &a : r.actions) if (a.operation == Operation::SetVelocity || a.operation == Operation::AddVelocity ||
            a.operation == Operation::SetAcceleration || a.operation == Operation::StopVelocity) return true;
    }
    return false;
}
constexpr size_t KeyCount = static_cast<size_t>(Key::F12) + 1;
struct InputSnapshot {
    std::array<bool, KeyCount> pressed{}, held{}, released{};
};
struct ProgramState {
    std::array<float, 3> position{}, rotation{}, scale{1, 1, 1}, velocity{}, acceleration{};
};
struct ProgramMemory { bool started = false; std::array<double, 16> timers{}; };
inline void TickProgram(const std::vector<Rule> &rules, ProgramMemory &memory, ProgramState &state, const InputSnapshot &input, double dt) {
    if (rules.empty() || rules.size() > 16 || !std::isfinite(dt) || dt < 0 || dt > 1) return;
    for (const auto &r : rules) if (!IsValidRule(r)) return;
    constexpr float radians = 0.017453292519943295f;
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto &r = rules[i];
        const auto key = static_cast<size_t>(r.key);
        bool fire = false;
        switch (r.event) {
        case Event::EveryFrame: fire = true; break;
        case Event::Start: fire = !memory.started; break;
        case Event::KeyPressed: fire = input.pressed[key]; break;
        case Event::KeyHeld: fire = input.held[key]; break;
        case Event::KeyReleased: fire = input.released[key]; break;
        case Event::Timer:
            memory.timers[i] += dt;
            if (memory.timers[i] >= r.interval) { fire = true; memory.timers[i] = std::fmod(memory.timers[i], r.interval); }
            break;
        }
        if (!fire) continue;
        for (const auto &c : r.conditions) {
            const auto axis = static_cast<size_t>(c.axis);
            switch (c.test) {
            case Test::PositionAtMost: fire &= state.position[axis] <= c.value; break;
            case Test::PositionAtLeast: fire &= state.position[axis] >= c.value; break;
            case Test::VelocityAtMost: fire &= state.velocity[axis] <= c.value; break;
            case Test::VelocityAtLeast: fire &= state.velocity[axis] >= c.value; break;
            }
        }
        if (!fire) continue;
        for (const auto &a : r.actions) {
            const auto axis = static_cast<size_t>(a.axis);
            const float step = static_cast<float>(dt);
            switch (a.operation) {
            case Operation::SetVelocity: state.velocity[axis] = a.value; break;
            case Operation::AddVelocity: state.velocity[axis] += a.value; break;
            case Operation::SetAcceleration: state.acceleration[axis] = a.value; break;
            case Operation::Translate: state.position[axis] += a.value * step; break;
            case Operation::Rotate: state.rotation[axis] += a.value * radians * step; break;
            case Operation::SetScale: state.scale[axis] = a.value; break;
            case Operation::SetPosition: state.position[axis] = a.value; break;
            case Operation::SetRotation: state.rotation[axis] = a.value * radians; break;
            case Operation::StopVelocity: state.velocity[axis] = 0; break;
            }
        }
    }
    memory.started = true;
}
} // namespace KashipanEngine::ShortText
