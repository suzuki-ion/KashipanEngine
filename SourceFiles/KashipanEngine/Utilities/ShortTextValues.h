#pragma once
#include <json.hpp>
#include <cmath>
#include <algorithm>
#include <random>
#include <stdexcept>

namespace KashipanEngine::ShortText {
using Value = nlohmann::json;
inline bool ValidValue(const Value &v, int depth = 0) {
    if (depth > 8) return false;
    if (v.is_null() || v.is_boolean()) return true;
    if (v.is_number()) return std::isfinite(v.get<double>()) && std::abs(v.get<double>()) <= 1e9;
    if (v.is_string()) return v.get_ref<const std::string &>().size() <= 512;
    if (v.is_array()) { if (v.size() > 64) return false; for (const auto &item : v) if (!ValidValue(item, depth + 1)) return false; return true; }
    if (v.is_object()) { if (v.size() > 32) return false; for (const auto &[key, item] : v.items()) if (key.size() > 128 || !ValidValue(item, depth + 1)) return false; return true; }
    return false;
}
inline Value ArithmeticValue(const std::string &op,const Value &a,const Value &b,int depth=0) {
    if(depth>8)throw std::runtime_error("arithmetic depth");
    if(a.is_array()||b.is_array()) {
        size_t count=a.is_array()?a.size():b.size();if(count>64||(a.is_array()&&b.is_array()&&a.size()!=b.size()))throw std::runtime_error("vector dimensions differ");
        Value result=Value::array();for(size_t i=0;i<count;++i)result.push_back(ArithmeticValue(op,a.is_array()?a[i]:a,b.is_array()?b[i]:b,depth+1));return result;
    }
    if(!a.is_number()||!b.is_number())throw std::runtime_error("numeric operand required");double x=a.get<double>(),y=b.get<double>(),v=0;
    if(op=="add")v=x+y;else if(op=="sub")v=x-y;else if(op=="mul")v=x*y;else if(op=="div"){if(y==0)throw std::runtime_error("division by zero");v=x/y;}else throw std::runtime_error("arithmetic operator");
    if(!std::isfinite(v)||std::abs(v)>1e9)throw std::runtime_error("numeric result out of range");return v;
}
// No code evaluation: a bounded expression tree with explicit operators.
inline Value Evaluate(const Value &expr, const Value &environment, int depth = 0) {
    if (!ValidValue(expr) || depth > 8) throw std::runtime_error("invalid expression");
    if (!expr.is_object()) return expr;
    if (expr.contains("read")) {
        const auto key = expr.at("read").get<std::string>();
        if (environment.contains(key)) return environment.at(key);
        const Value *current = &environment;
        size_t start = 0;
        for (;;) { const auto end = key.find('.', start); const auto part = key.substr(start, end - start);
            if (current->is_array()) {
                if (part.empty() || part.size() > 6 || part.find_first_not_of("0123456789") != std::string::npos) return Value();
                const auto index = static_cast<size_t>(std::stoul(part));
                if (index >= current->size()) return Value();
                current = &current->at(index);
            } else {
                if (!current->is_object() || !current->contains(part)) return Value(); current = &current->at(part);
            }
            if (end == std::string::npos) return *current; start = end + 1; }
    }
    if (expr.contains("literal")) return expr.at("literal");
    if (!expr.contains("op")) return expr;
    const auto op = expr.at("op").get<std::string>();
    const auto &args = expr.at("args");
    if (!args.is_array() || args.size() > 8) throw std::runtime_error("expression arguments");
    if (op == "and" || op == "or") {
        for (const auto &arg : args) { const auto value = Evaluate(arg, environment, depth + 1); if (!value.is_boolean()) throw std::runtime_error("boolean operand required");
            if(op=="and"&&!value.get<bool>())return false;
            if(op=="or"&&value.get<bool>())return true; }
        return op=="and";
    }
    auto at = [&](size_t i) { return Evaluate(args.at(i), environment, depth + 1); };
    if (op == "not") return !at(0).get<bool>();
    if (op == "exists") return !at(0).is_null();
    if (op == "eq") return at(0) == at(1);
    if (op == "ne") return at(0) != at(1);
    if (op == "get") {
        const auto collection = at(0), key = at(1);
        if (collection.is_object() && key.is_string()) return collection.value(key.get<std::string>(), Value());
        if (collection.is_array() && key.is_number()) {
            const double index = key.get<double>();
            if (index >= 0 && index == std::floor(index) && index < static_cast<double>(collection.size())) return collection.at(static_cast<size_t>(index));
            return Value();
        }
        return Value();
    }
    if (op == "contains") {auto collection=at(0),value=at(1);if(collection.is_array())return std::find(collection.begin(),collection.end(),value)!=collection.end();if(collection.is_string()&&value.is_string())return collection.get<std::string>().find(value.get<std::string>())!=std::string::npos;throw std::runtime_error("contains needs array or string");}
    if (op == "concat") { std::string text; for (const auto &arg : args) { auto value = Evaluate(arg, environment, depth + 1); text += value.is_string() ? value.get<std::string>() : value.dump(); } if (text.size() > 512) throw std::runtime_error("text too long"); return text; }
    if(op=="add"||op=="sub"||op=="mul"||op=="div")return ArithmeticValue(op,at(0),at(1));
    auto number = [&](size_t i) { const auto value = at(i); if (!value.is_number()) throw std::runtime_error("numeric operand required"); return value.get<double>(); };
    double value = 0;
    if (op == "min") value = std::min(number(0), number(1));
    else if (op == "max") value = std::max(number(0), number(1));
    else if (op == "clamp") { const double lo = number(1), hi = number(2); if (lo > hi) throw std::runtime_error("clamp bounds"); value = std::clamp(number(0), lo, hi); }
    else if (op == "lt") return number(0) < number(1);
    else if (op == "le") return number(0) <= number(1);
    else if (op == "gt") return number(0) > number(1);
    else if (op == "ge") return number(0) >= number(1);
    else if (op == "between") { const double v = number(0); return v >= number(1) && v <= number(2); }
    else if (op == "random") { static thread_local std::mt19937 generator(0x4b5345); const double lo = number(0), hi = number(1); if (lo > hi) throw std::runtime_error("random bounds"); value = std::uniform_real_distribution<double>(lo, hi)(generator); }
    else if (op == "vector" || op == "color") { Value v = Value::array(); for (const auto &arg : args) v.push_back(Evaluate(arg, environment, depth + 1)); return v; }
    else if (op == "component") {double index=number(1);auto collection=at(0);if(index<0||index!=std::floor(index)||index>1024||!collection.is_array()||index>=collection.size())throw std::runtime_error("component index");return collection.at(static_cast<size_t>(index));}
    else throw std::runtime_error("unknown expression operator");
    if (!std::isfinite(value) || std::abs(value) > 1e9) throw std::runtime_error("numeric result out of range");
    return value;
}
} // namespace KashipanEngine::ShortText
