#pragma once
#include <json.hpp>
#include "Utilities/ShortTextBehavior.h"

namespace KashipanEngine::ShortText {
inline int ReadInteger(const nlohmann::json &value, int min, int max) {
    if (!value.is_number_integer()) throw std::runtime_error("integer expected");
    if (value.is_number_unsigned()) {
        if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(max)) throw std::runtime_error("integer overflow");
    } else {
        const auto v = value.get<std::int64_t>();
        if (v < min || v > max) throw std::runtime_error("integer out of range");
    }
    return value.get<int>();
}
template <size_t N> int ReadName(const nlohmann::json &value, const std::array<const char *, N> &names) {
    const auto name = value.get<std::string>();
    for (size_t i = 0; i < N; ++i) if (name == names[i]) return static_cast<int>(i);
    throw std::runtime_error("unknown operation name");
}
inline nlohmann::json EncodeRecipe(const Recipe &r) {
    if (!IsValid(r)) throw std::runtime_error("invalid recipe");
    if (r.kind != Kind::Rules) return {{"kind", static_cast<int>(r.kind)}, {"axis", r.axis}, {"direction", r.direction},
        {"amplitude", r.amplitude}, {"period", r.period}, {"speed", r.speed}};
    auto rules = nlohmann::json::array();
    for (const auto &rule : r.rules) {
        auto conditions = nlohmann::json::array(), actions = nlohmann::json::array();
        for (const auto &c : rule.conditions) conditions.push_back({{"test", TestNames[static_cast<size_t>(c.test)]}, {"axis", c.axis}, {"value", c.value}, {"name", c.name}, {"data", c.data}});
        for (const auto &a : rule.actions) actions.push_back({{"operation", OperationNames[static_cast<size_t>(a.operation)]}, {"axis", a.axis}, {"value", a.value}, {"name", a.name}, {"target", TargetNames[static_cast<size_t>(a.target)]}, {"targetName", a.targetName}, {"reference", a.reference}, {"text", a.text}, {"duration", a.duration}, {"data", a.data}});
        rules.push_back({{"event", EventNames[static_cast<size_t>(rule.event)]}, {"key", KeyName(rule.key)}, {"interval", rule.interval}, {"tag", rule.tag}, {"button", rule.button}, {"once", rule.once}, {"variable", rule.variable}, {"cooldown", rule.cooldown}, {"state", rule.state}, {"windowStart", rule.windowStart}, {"windowEnd", rule.windowEnd}, {"conditions", conditions}, {"actions", actions}});
    }
    return {{"kind", 3}, {"rules", rules}};
}
inline Recipe DecodeRecipe(const nlohmann::json &json) {
    Recipe r;
    if (!json.is_object()) throw std::runtime_error("recipe object expected");
    r.kind = static_cast<Kind>(ReadInteger(json.value("kind", nlohmann::json(0)), 0, 3));
    if (r.kind == Kind::Rules) {
        const auto &rules = json.at("rules");
        if (!rules.is_array() || rules.empty() || rules.size() > 16) throw std::runtime_error("rule count");
        for (const auto &item : rules) {
            Rule rule;
            rule.event = static_cast<Event>(ReadName(item.at("event"), EventNames));
            rule.key = ParseKey(item.value("key", std::string("Space")));
            rule.interval = item.value("interval", 1.0f);
            rule.tag = item.value("tag", std::string{});
            rule.button = ReadInteger(item.value("button", nlohmann::json(0)), 0, 2);
            rule.once = item.value("once", false); rule.variable = item.value("variable", std::string{});
            rule.cooldown = item.value("cooldown", 0.0f); rule.state = item.value("state", std::string{});
            rule.windowStart = item.value("windowStart", 0.0f); rule.windowEnd = item.value("windowEnd", -1.0f);
            const auto &conditions = item.at("conditions"), &actions = item.at("actions");
            if (!conditions.is_array() || conditions.size() > 4 || !actions.is_array() || actions.empty() || actions.size() > 8) throw std::runtime_error("condition/action count");
            for (const auto &c : conditions) {
                Condition condition;
                condition.test = static_cast<Test>(ReadName(c.at("test"), TestNames));
                condition.axis = ReadInteger(c.value("axis", nlohmann::json(0)), 0, 2);
                condition.value = c.value("value", 0.0f); condition.name = c.value("name", std::string{});
                condition.data = c.value("data", Value::object());
                rule.conditions.push_back(std::move(condition));
            }
            for (const auto &a : actions) {
                Action action; action.operation = static_cast<Operation>(ReadName(a.at("operation"), OperationNames));
                action.axis = ReadInteger(a.value("axis", nlohmann::json(0)), 0, 2); action.value = a.value("value", 0.0f); action.name = a.value("name", std::string{});
                action.target = static_cast<Target>(ReadName(a.value("target", nlohmann::json("self")), TargetNames));
                action.targetName = a.value("targetName", std::string{}); action.reference = a.value("reference", std::string{});
                action.text = a.value("text", std::string{}); action.duration = a.value("duration", 1.0f);
                action.data = a.value("data", Value::object());
                rule.actions.push_back(std::move(action));
            }
            r.rules.push_back(std::move(rule));
        }
    } else {
        r.axis = ReadInteger(json.value("axis", nlohmann::json(1)), 0, 2);
        r.direction = ReadInteger(json.value("direction", nlohmann::json(1)), -1, 1);
        r.amplitude = json.value("amplitude", 0.5f);
        r.period = json.value("period", 2.0f);
        r.speed = json.value("speed", 1.0f);
        if (json.contains("rules")) throw std::runtime_error("rules in a simple recipe");
    }
    if (!IsValid(r)) throw std::runtime_error("invalid recipe fields");
    return r;
}
} // namespace KashipanEngine::ShortText
