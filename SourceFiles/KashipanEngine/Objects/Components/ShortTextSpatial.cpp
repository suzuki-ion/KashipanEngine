#include "Objects/Components/ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>

namespace KashipanEngine::ShortText {
namespace {
using Vec3d = std::array<double, 3>;
constexpr double kMaxCoordinate = 1e7;
constexpr double kMaxDerived = 1e9;
constexpr double kCoincidentEpsilon = 1e-9;
constexpr size_t kMaxResultNameBytes = 64;
constexpr size_t kMaxReferenceBytes = 256;
constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

[[noreturn]] void Fail(const std::string &message) { throw std::runtime_error(message); }

void RequireFiniteBounded(double v, double bound, const char *what) {
    if (!std::isfinite(v) || std::abs(v) > bound) Fail(std::string(what) + " must be finite and within +/-" + std::to_string(bound));
}

Vec3d ReadPosition(const Value &v) {
    if (!v.is_array() || v.size() != 3) Fail("position requires an array of exactly 3 numbers");
    Vec3d r{};
    for (size_t i = 0; i < 3; ++i) {
        const Value &e = v.at(i);
        if (!e.is_number() || e.is_boolean()) Fail("position elements must be numbers");
        r[i] = e.get<double>();
        RequireFiniteBounded(r[i], kMaxCoordinate, "position element");
    }
    return r;
}

int ReadDimension(const Value &a) {
    if (!a.contains("dimension")) return 3;
    const Value &v = a.at("dimension");
    // Floats (even 2.0) are rejected so fractional values are never truncated.
    if (!v.is_number_integer() || v.is_boolean()) Fail("dimension must be the integer 2 or 3");
    const std::int64_t d = v.is_number_unsigned() ? (v.get<std::uint64_t>() > 3 ? 0 : static_cast<std::int64_t>(v.get<std::uint64_t>())) : v.get<std::int64_t>();
    if (d != 2 && d != 3) Fail("dimension must be the integer 2 or 3");
    return static_cast<int>(d);
}

std::string ReadResultName(const Value &a) {
    if (!a.contains("result")) return "spatial";
    const Value &v = a.at("result");
    if (!v.is_string()) Fail("result must be a string");
    std::string name = v.get<std::string>();
    if (name.empty() || name.size() > kMaxResultNameBytes) Fail("result name must be 1..64 bytes");
    for (unsigned char ch : name) if (ch < 0x20 || ch == 0x7F) Fail("result name must not contain control characters");
    return name;
}

Vec3d WorldPosition(Transform &t, const char *what) {
    const auto world = t.GetWorldMatrix();
    Vec3d r{static_cast<double>(world.m[3][0]), static_cast<double>(world.m[3][1]), static_cast<double>(world.m[3][2])};
    for (double v : r) RequireFiniteBounded(v, kMaxCoordinate, what);
    return r;
}

// Stable UUID wins; otherwise the exact name must identify exactly one object.
EmptyObject *ResolveReference(SceneContext &scene, const std::string &reference) {
    for (auto *o : scene.GetSceneObjects()) if (o && o->GetObjectID().ToString() == reference) return o;
    EmptyObject *found = nullptr;
    for (auto *o : scene.GetSceneObjects()) {
        if (!o || o->GetName() != reference) continue;
        if (found) Fail("reference name '" + reference + "' is ambiguous; use the object UUID");
        found = o;
    }
    if (!found) Fail("reference '" + reference + "' does not match any scene object UUID or name");
    return found;
}

Vec3d ResolveDestination(const Value &a, SceneContext &scene) {
    const bool hasReference = a.contains("reference");
    const bool hasPosition = a.contains("position");
    if (hasReference == hasPosition) Fail("specify exactly one of reference or position");
    if (hasPosition) return ReadPosition(a.at("position"));
    const Value &v = a.at("reference");
    if (!v.is_string()) Fail("reference must be a string");
    const std::string reference = v.get<std::string>();
    if (reference.empty() || reference.size() > kMaxReferenceBytes) Fail("reference must be 1..256 bytes");
    auto *object = ResolveReference(scene, reference);
    auto *transform = object->GetComponent<Transform>();
    if (!transform) Fail("reference object requires Transform");
    return WorldPosition(*transform, "reference world position");
}

Value Array3(const Vec3d &v) { return Value::array({v[0], v[1], v[2]}); }

Value Compute(const Vec3d &source, const Vec3d &destination, int dimension) {
    Vec3d delta{destination[0] - source[0], destination[1] - source[1], destination[2] - source[2]};
    if (dimension == 2) delta[2] = 0.0;
    const double distance = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    RequireFiniteBounded(distance, kMaxDerived, "distance");
    const bool coincident = distance <= kCoincidentEpsilon;
    Vec3d direction{};
    if (!coincident) for (size_t i = 0; i < 3; ++i) direction[i] = delta[i] / distance;
    for (size_t i = 0; i < 3; ++i) {
        RequireFiniteBounded(delta[i], kMaxDerived, "delta");
        RequireFiniteBounded(direction[i], kMaxDerived, "direction");
    }
    Value result = Value::object();
    result["distance"] = distance;
    result["delta"] = Array3(delta);
    result["direction"] = Array3(direction);
    result["coincident"] = coincident;
    if (dimension == 2) {
        const double angle = coincident ? 0.0 : std::atan2(delta[1], delta[0]) * kRadiansToDegrees;
        RequireFiniteBounded(angle, kMaxDerived, "angle");
        result["angle_degrees"] = angle;
    } else {
        const double yaw = coincident ? 0.0 : std::atan2(delta[0], delta[2]) * kRadiansToDegrees;
        const double pitch = coincident ? 0.0 : std::atan2(delta[1], std::hypot(delta[0], delta[2])) * kRadiansToDegrees;
        RequireFiniteBounded(yaw, kMaxDerived, "yaw");
        RequireFiniteBounded(pitch, kMaxDerived, "pitch");
        result["yaw_degrees"] = yaw;
        result["pitch_degrees"] = pitch;
    }
    return result;
}
}

bool ExecuteSpatialCommand(const std::string &command, const Value &a, CommandContext &c, std::string &error) {
    error.clear();
    if (command != "spatial.direction" && command != "spatial.distance") return false;
    try {
        if (!a.is_object()) Fail("arguments must be object");
        for (const auto &item : a.items()) {
            const std::string &key = item.key();
            if (key != "reference" && key != "position" && key != "dimension" && key != "result") Fail("unknown argument '" + key + "'");
        }
        if (!c.scene) Fail("scene required");
        if (!c.target) Fail("target required");
        if (!c.memory) Fail("program memory required");
        auto *sourceTransform = c.target->GetComponent<Transform>();
        if (!sourceTransform) Fail("target requires Transform");

        const std::string resultName = ReadResultName(a);
        const int dimension = ReadDimension(a);
        const Vec3d destination = ResolveDestination(a, *c.scene);
        const Vec3d source = WorldPosition(*sourceTransform, "target world position");
        Value result = Compute(source, destination, dimension);

        // Validate the existing store without creating entries, so failures leave memory untouched.
        auto &values = c.memory->extensionValues;
        const bool hasResults = values.contains("results");
        if (hasResults && !values["results"].is_object()) Fail("existing results store is malformed (not an object)");
        values["results"][resultName] = std::move(result);
        return true;
    } catch (const std::exception &e) {
        error = command + ": " + e.what();
        return false;
    }
}
} // namespace KashipanEngine::ShortText
