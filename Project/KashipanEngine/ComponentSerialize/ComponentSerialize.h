#pragma once
#include <Math/Vector2.h>
#include <Math/Vector3.h>
#include <Math/Vector4.h>
#include <Math/Matrix3x3.h>
#include <Math/Matrix4x4.h>
#include <Math/Quaternion.h>
#include <string>
#include <vector>
#include <Utilities/FileIO/JSON.h>

namespace KashipanEngine {

struct FieldInfo {
    const char *name;
    size_t offset;
    enum class Type {
        Int,
        Float,
        String,
        Bool,
        Vector2,
        Vector3,
        Vector4,
        Matrix3x3,
        Matrix4x4,
        Quaternion
    } type;
};

template <typename T>
constexpr FieldInfo::Type FieldTypeOf() {
    return FieldInfo::Type::Int;
}

template <>
constexpr FieldInfo::Type FieldTypeOf<int>() { return FieldInfo::Type::Int; }
template <>
constexpr FieldInfo::Type FieldTypeOf<float>() { return FieldInfo::Type::Float; }
template <>
constexpr FieldInfo::Type FieldTypeOf<std::string>() { return FieldInfo::Type::String; }
template <>
constexpr FieldInfo::Type FieldTypeOf<bool>() { return FieldInfo::Type::Bool; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Vector2>() { return FieldInfo::Type::Vector2; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Vector3>() { return FieldInfo::Type::Vector3; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Vector4>() { return FieldInfo::Type::Vector4; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Matrix3x3>() { return FieldInfo::Type::Matrix3x3; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Matrix4x4>() { return FieldInfo::Type::Matrix4x4; }
template <>
constexpr FieldInfo::Type FieldTypeOf<Quaternion>() { return FieldInfo::Type::Quaternion; }

#define FIELD(type, member) \
    FieldInfo{#member, offsetof(type, member), FieldTypeOf<decltype(type::member)>()}

#define REFLECT(...) \
    static std::vector<FieldInfo> Reflect() { \
        return {__VA_ARGS__}; \
    }

void SerializeComponent(const void *component, const std::vector<FieldInfo> &fields, json &outJson);
void DeserializeComponent(void *component, const std::vector<FieldInfo> &fields, const json &inJson);

} // namespace KashipanEngine