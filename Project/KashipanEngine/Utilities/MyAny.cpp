#include "MyAny.h"

#include "Debug/Logger.h"

namespace KashipanEngine {

JSON SaveAnyToJson(const MyAny &value) {
    LogScope scope;

    if (value.IsEmpty()) return JSON();
    const TypeInfo &typeInfo = value.GetTypeInfo();
    if (typeInfo.GetBaseType() == ValueType::None) return JSON();
    else if (typeInfo.GetBaseType() == ValueType::Bool) return ToJSON(value.AnyCast<bool>());
    else if (typeInfo.GetBaseType() == ValueType::Int8) return ToJSON(value.AnyCast<int8_t>());
    else if (typeInfo.GetBaseType() == ValueType::UInt8) return ToJSON(value.AnyCast<uint8_t>());
    else if (typeInfo.GetBaseType() == ValueType::Int16) return ToJSON(value.AnyCast<int16_t>());
    else if (typeInfo.GetBaseType() == ValueType::UInt16) return ToJSON(value.AnyCast<uint16_t>());
    else if (typeInfo.GetBaseType() == ValueType::Int32) return ToJSON(value.AnyCast<int32_t>());
    else if (typeInfo.GetBaseType() == ValueType::UInt32) return ToJSON(value.AnyCast<uint32_t>());
    else if (typeInfo.GetBaseType() == ValueType::Int64) return ToJSON(value.AnyCast<int64_t>());
    else if (typeInfo.GetBaseType() == ValueType::UInt64) return ToJSON(value.AnyCast<uint64_t>());
    else if (typeInfo.GetBaseType() == ValueType::Float) return ToJSON(value.AnyCast<float>());
    else if (typeInfo.GetBaseType() == ValueType::Double) return ToJSON(value.AnyCast<double>());
    else if (typeInfo.GetBaseType() == ValueType::String) return ToJSON(value.AnyCast<std::string>());
    else if (typeInfo.GetBaseType() == ValueType::Vector2) return ToJSON(value.AnyCast<Vector2>());
    else if (typeInfo.GetBaseType() == ValueType::Vector3) return ToJSON(value.AnyCast<Vector3>());
    else if (typeInfo.GetBaseType() == ValueType::Vector4) return ToJSON(value.AnyCast<Vector4>());
    else if (typeInfo.GetBaseType() == ValueType::Matrix3x3) return ToJSON(value.AnyCast<Matrix3x3>());
    else if (typeInfo.GetBaseType() == ValueType::Matrix4x4) return ToJSON(value.AnyCast<Matrix4x4>());
    else if (typeInfo.GetBaseType() == ValueType::Quaternion) return ToJSON(value.AnyCast<Quaternion>());
    else if (typeInfo.GetBaseType() == ValueType::Color) return ToJSON(value.AnyCast<Color>());
    else if (typeInfo.GetBaseType() == ValueType::TextureRef) return ToJSON(value.AnyCast<TextureRef>());
    else if (typeInfo.GetBaseType() == ValueType::TextureCubeRef) return ToJSON(value.AnyCast<TextureCubeRef>());
    else if (typeInfo.GetBaseType() == ValueType::Vector) {
        JSON json = JSON::array();
        // std::vector の場合、内部型を取得して再帰的に保存
        const auto &templateArgs = typeInfo.GetTemplateArguments();
        if (!templateArgs.empty()) {
            const auto &vec = value.AnyCast<std::vector<MyAny>>();
            for (const auto &elem : vec) {
                json.push_back(SaveAnyToJson(elem));
            }
            return json;
        }
    } else if (typeInfo.GetBaseType() == ValueType::UnorderedMap) {
        JSON json = JSON::object();
        // std::unordered_map の場合、キーと値の型を取得して再帰的に保存
        const auto &templateArgs = typeInfo.GetTemplateArguments();
        if (templateArgs.size() >= 2) {
            const auto &map = value.AnyCast<std::unordered_map<MyAny, MyAny>>();
            for (const auto &[key, val] : map) {
                JSON keyJson = SaveAnyToJson(key);
                JSON valueJson = SaveAnyToJson(val);
                json[keyJson] = valueJson;
            }
            return json;
        }
    } else if (typeInfo.GetBaseType() == ValueType::Pair) {
        JSON json = JSON::array();
        const auto &pair = value.AnyCast<std::pair<MyAny, MyAny>>();
        json.push_back(SaveAnyToJson(pair.first));
        json.push_back(SaveAnyToJson(pair.second));
        return json;
    }
    return JSON();
}

MyAny LoadAnyFromJson(const JSON &json, const TypeInfo &typeInfo) {
    LogScope scope;

    if (typeInfo.GetBaseType() == ValueType::None) return MyAny();
    else if (typeInfo.GetBaseType() == ValueType::Bool) return MyAny(FromJSON<bool>(json));
    else if (typeInfo.GetBaseType() == ValueType::Int8) return MyAny(FromJSON<int8_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::UInt8) return MyAny(FromJSON<uint8_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::Int16) return MyAny(FromJSON<int16_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::UInt16) return MyAny(FromJSON<uint16_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::Int32) return MyAny(FromJSON<int32_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::UInt32) return MyAny(FromJSON<uint32_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::Int64) return MyAny(FromJSON<int64_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::UInt64) return MyAny(FromJSON<uint64_t>(json));
    else if (typeInfo.GetBaseType() == ValueType::Float) return MyAny(FromJSON<float>(json));
    else if (typeInfo.GetBaseType() == ValueType::Double) return MyAny(FromJSON<double>(json));
    else if (typeInfo.GetBaseType() == ValueType::String) return MyAny(FromJSON<std::string>(json));
    else if (typeInfo.GetBaseType() == ValueType::Vector2) return MyAny(FromJSON<Vector2>(json));
    else if (typeInfo.GetBaseType() == ValueType::Vector3) return MyAny(FromJSON<Vector3>(json));
    else if (typeInfo.GetBaseType() == ValueType::Vector4) return MyAny(FromJSON<Vector4>(json));
    else if (typeInfo.GetBaseType() == ValueType::Matrix3x3) return MyAny(FromJSON<Matrix3x3>(json));
    else if (typeInfo.GetBaseType() == ValueType::Matrix4x4) return MyAny(FromJSON<Matrix4x4>(json));
    else if (typeInfo.GetBaseType() == ValueType::Quaternion) return MyAny(FromJSON<Quaternion>(json));
    else if (typeInfo.GetBaseType() == ValueType::Color) return MyAny(FromJSON<Color>(json));
    else if (typeInfo.GetBaseType() == ValueType::TextureRef) return MyAny(FromJSON<TextureRef>(json));
    else if (typeInfo.GetBaseType() == ValueType::TextureCubeRef) return MyAny(FromJSON<TextureCubeRef>(json));
    else if (typeInfo.GetBaseType() == ValueType::Vector) {
        const auto &templateArgs = typeInfo.GetTemplateArguments();
        if (!templateArgs.empty()) {
            const TypeInfo &innerTypeInfo = templateArgs[0];
            std::vector<MyAny> vec;
            for (const auto &elemJson : json) {
                vec.push_back(LoadAnyFromJson(elemJson, innerTypeInfo));
            }
            return MyAny(vec, typeInfo);
        }
    } else if (typeInfo.GetBaseType() == ValueType::UnorderedMap) {
        const auto &templateArgs = typeInfo.GetTemplateArguments();
        if (templateArgs.size() >= 2) {
            const TypeInfo &keyTypeInfo = templateArgs[0];
            const TypeInfo &valueTypeInfo = templateArgs[1];
            std::unordered_map<MyAny, MyAny> map;
            for (auto it = json.begin(); it != json.end(); ++it) {
                MyAny key = LoadAnyFromJson(JSON(it.key()), keyTypeInfo);
                MyAny val = LoadAnyFromJson(it.value(), valueTypeInfo);
                map[key] = val;
            }
            return MyAny(map, typeInfo);
        }
    } else if (typeInfo.GetBaseType() == ValueType::Pair) {
        const auto &templateArgs = typeInfo.GetTemplateArguments();
        if (templateArgs.size() >= 2) {
            const TypeInfo &firstTypeInfo = templateArgs[0];
            const TypeInfo &secondTypeInfo = templateArgs[1];
            MyAny first = LoadAnyFromJson(json[0], firstTypeInfo);
            MyAny second = LoadAnyFromJson(json[1], secondTypeInfo);
            return MyAny(std::make_pair(first, second), typeInfo);
        }
    }
    return MyAny();
}

} // namespace KashipanEngine
