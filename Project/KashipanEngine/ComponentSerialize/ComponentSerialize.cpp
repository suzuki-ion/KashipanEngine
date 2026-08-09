#include "ComponentSerialize.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SerializeComponent(const void *component, const std::vector<FieldInfo> &fields, json &outJson) {
    LogScope scope;
    char *base = static_cast<char *>(const_cast<void *>(component));
    for (const auto &field : fields) {
        const void *fieldPtr = base + field.offset;
        switch (field.type) {
            case FieldInfo::Type::Int:
                outJson[field.parentTypeName][field.name] = *static_cast<const int *>(fieldPtr);
                break;
            case FieldInfo::Type::Float:
                outJson[field.parentTypeName][field.name] = *static_cast<const float *>(fieldPtr);
                break;
            case FieldInfo::Type::String:
                outJson[field.parentTypeName][field.name] = *static_cast<const std::string *>(fieldPtr);
                break;
            case FieldInfo::Type::Bool:
                outJson[field.parentTypeName][field.name] = *static_cast<const bool *>(fieldPtr);
                break;
            case FieldInfo::Type::Vector2:
                outJson[field.parentTypeName][field.name]["x"] = static_cast<const Vector2 *>(fieldPtr)->x;
                outJson[field.parentTypeName][field.name]["y"] = static_cast<const Vector2 *>(fieldPtr)->y;
                break;
            case FieldInfo::Type::Vector3:
                outJson[field.parentTypeName][field.name]["x"] = static_cast<const Vector3 *>(fieldPtr)->x;
                outJson[field.parentTypeName][field.name]["y"] = static_cast<const Vector3 *>(fieldPtr)->y;
                outJson[field.parentTypeName][field.name]["z"] = static_cast<const Vector3 *>(fieldPtr)->z;
                break;
            case FieldInfo::Type::Vector4:
                outJson[field.parentTypeName][field.name]["x"] = static_cast<const Vector4 *>(fieldPtr)->x;
                outJson[field.parentTypeName][field.name]["y"] = static_cast<const Vector4 *>(fieldPtr)->y;
                outJson[field.parentTypeName][field.name]["z"] = static_cast<const Vector4 *>(fieldPtr)->z;
                outJson[field.parentTypeName][field.name]["w"] = static_cast<const Vector4 *>(fieldPtr)->w;
                break;
            case FieldInfo::Type::Matrix3x3:
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        outJson[field.parentTypeName][field.name][i][j] = static_cast<const Matrix3x3 *>(fieldPtr)->m[i][j];
                    }
                }
                break;
            case FieldInfo::Type::Matrix4x4:
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        outJson[field.parentTypeName][field.name][i][j] = static_cast<const Matrix4x4 *>(fieldPtr)->m[i][j];
                    }
                }
                break;
            case FieldInfo::Type::Quaternion:
                outJson[field.parentTypeName][field.name]["x"] = static_cast<const Quaternion *>(fieldPtr)->x;
                outJson[field.parentTypeName][field.name]["y"] = static_cast<const Quaternion *>(fieldPtr)->y;
                outJson[field.parentTypeName][field.name]["z"] = static_cast<const Quaternion *>(fieldPtr)->z;
                outJson[field.parentTypeName][field.name]["w"] = static_cast<const Quaternion *>(fieldPtr)->w;
                break;
            default:
                Log(Translation("engine.componentserialize.serialize.failed.unsupportedtype") + field.name, LogSeverity::Warning);
                break;
        }
    }
}

void DeserializeComponent(void *component, const std::vector<FieldInfo> &fields, const json &inJson) {
    char *base = static_cast<char *>(component);
    for (const auto &field : fields) {
        void *fieldPtr = base + field.offset;
        if (!inJson.contains(field.name)) {
            Log(Translation("engine.componentserialize.deserialize.field.notfound") + field.name, LogSeverity::Warning);
            continue;
        }
        switch (field.type) {
            case FieldInfo::Type::Int:
                *static_cast<int *>(fieldPtr) = inJson[field.parentTypeName][field.name].get<int>();
                break;
            case FieldInfo::Type::Float:
                *static_cast<float *>(fieldPtr) = inJson[field.parentTypeName][field.name].get<float>();
                break;
            case FieldInfo::Type::String:
                *static_cast<std::string *>(fieldPtr) = inJson[field.parentTypeName][field.name].get<std::string>();
                break;
            case FieldInfo::Type::Bool:
                *static_cast<bool *>(fieldPtr) = inJson[field.parentTypeName][field.name].get<bool>();
                break;
            case FieldInfo::Type::Vector2: {
                *static_cast<Vector2 *>(fieldPtr) = Vector2(
                    inJson[field.parentTypeName][field.name]["x"].get<float>(),
                    inJson[field.parentTypeName][field.name]["y"].get<float>());
                break;
            }
            case FieldInfo::Type::Vector3: {
                *static_cast<Vector3 *>(fieldPtr) = Vector3(
                    inJson[field.parentTypeName][field.name]["x"].get<float>(),
                    inJson[field.parentTypeName][field.name]["y"].get<float>(),
                    inJson[field.parentTypeName][field.name]["z"].get<float>()
                );
                break;
            }
            case FieldInfo::Type::Vector4: {
                *static_cast<Vector4 *>(fieldPtr) = Vector4(
                    inJson[field.parentTypeName][field.name]["x"].get<float>(),
                    inJson[field.parentTypeName][field.name]["y"].get<float>(),
                    inJson[field.parentTypeName][field.name]["z"].get<float>(),
                    inJson[field.parentTypeName][field.name]["w"].get<float>()
                );
                break;
            }
            case FieldInfo::Type::Matrix3x3: {
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        *static_cast<float *>(&static_cast<Matrix3x3 *>(fieldPtr)->m[i][j]) = inJson[field.parentTypeName][field.name][i][j].get<float>();
                    }
                }
                break;
            }
            case FieldInfo::Type::Matrix4x4: {
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        *static_cast<float *>(&static_cast<Matrix4x4 *>(fieldPtr)->m[i][j]) = inJson[field.parentTypeName][field.name][i][j].get<float>();
                    }
                }
                break;
            }
            case FieldInfo::Type::Quaternion: {
                *static_cast<float *>(&static_cast<Quaternion *>(fieldPtr)->x) = inJson[field.parentTypeName][field.name]["x"].get<float>();
                *static_cast<float *>(&static_cast<Quaternion *>(fieldPtr)->y) = inJson[field.parentTypeName][field.name]["y"].get<float>();
                *static_cast<float *>(&static_cast<Quaternion *>(fieldPtr)->z) = inJson[field.parentTypeName][field.name]["z"].get<float>();
                *static_cast<float *>(&static_cast<Quaternion *>(fieldPtr)->w) = inJson[field.parentTypeName][field.name]["w"].get<float>();
                break;
            }
            default:
                Log(Translation("engine.componentserialize.deserialize.failed.unsupportedtype") + field.name, LogSeverity::Warning);
                break;
        }
    }
}

} // namespace KashipanEngine