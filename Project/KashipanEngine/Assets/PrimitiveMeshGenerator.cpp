#include "PrimitiveMeshGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Assets/ModelManager.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"

namespace KashipanEngine {

namespace {

using Vertex = ModelData::Vertex;

Vertex MakeVertex(const Vector3 &position, const Vector3 &normal, float u, float v) {
    Vertex vertex{};
    vertex.px = position.x;
    vertex.py = position.y;
    vertex.pz = position.z;
    vertex.nx = normal.x;
    vertex.ny = normal.y;
    vertex.nz = normal.z;
    vertex.u = u;
    vertex.v = v;
    return vertex;
}

/// @brief p0,p1,p2,p3 を外側から見て反時計回り(CCW)の順で渡すと、平坦な法線を計算した上で
///        そのままの順でインデックスを積む（このエンジンの既定は反時計回りが表面のため）
void AddQuad(std::vector<Vertex> &verts, std::vector<uint32_t> &idx,
    const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3) {
    const Vector3 normal = (p1 - p0).Cross(p2 - p0).Normalize();
    const uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back(MakeVertex(p0, normal, 0.0f, 1.0f));
    verts.push_back(MakeVertex(p1, normal, 1.0f, 1.0f));
    verts.push_back(MakeVertex(p2, normal, 1.0f, 0.0f));
    verts.push_back(MakeVertex(p3, normal, 0.0f, 0.0f));
    idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
    idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
}

/// @brief p0,p1,p2 を外側から見てCCW順で渡すと、平坦な法線を計算した上でそのままの順で積む
void AddTriangle(std::vector<Vertex> &verts, std::vector<uint32_t> &idx,
    const Vector3 &p0, const Vector3 &p1, const Vector3 &p2) {
    const Vector3 normal = (p1 - p0).Cross(p2 - p0).Normalize();
    const uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back(MakeVertex(p0, normal, 0.5f, 0.0f));
    verts.push_back(MakeVertex(p1, normal, 0.0f, 1.0f));
    verts.push_back(MakeVertex(p2, normal, 1.0f, 1.0f));
    idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
}

/// @brief 平面上の正多角形（円/正三角形など）を構築するための基底
struct PlaneBasis {
    Vector3 normal;
    Vector3 uAxis;
    Vector3 vAxis;
};

/// @brief uAxis.Cross(vAxis) == normal となる基底を返す（Circle/Triangleで共用する）
PlaneBasis GetPlaneBasis(char orientation) {
    switch (orientation) {
    case 'Z': // XZ平面。法線は+Y
        return { Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f) };
    case 'Y': // XY平面。法線は+Z
        return { Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f) };
    case 'X': // YZ平面。法線は+X
    default:
        return { Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f) };
    }
}

/// @brief 平面プリミティブ（1x1の平らな四角形）
void GeneratePlane(std::vector<Vertex> &verts, std::vector<uint32_t> &idx, char orientation) {
    const PlaneBasis basis = GetPlaneBasis(orientation);
    const float h = 0.5f;
    const Vector3 p0 = basis.uAxis * -h + basis.vAxis * -h;
    const Vector3 p1 = basis.uAxis * -h + basis.vAxis * h;
    const Vector3 p2 = basis.uAxis * h + basis.vAxis * h;
    const Vector3 p3 = basis.uAxis * h + basis.vAxis * -h;
    AddQuad(verts, idx, p0, p1, p2, p3);
}

/// @brief 正三角形プリミティブ（外接円半径0.5）
void GenerateTriangle(std::vector<Vertex> &verts, std::vector<uint32_t> &idx, char orientation) {
    const PlaneBasis basis = GetPlaneBasis(orientation);
    const float radius = 0.5f;
    const float pi = GetPI<float>();
    Vector3 points[3];
    for (int i = 0; i < 3; ++i) {
        const float theta = pi * 0.5f + static_cast<float>(i) * (2.0f * pi / 3.0f);
        points[i] = basis.uAxis * (radius * std::cos(theta)) + basis.vAxis * (radius * std::sin(theta));
    }
    AddTriangle(verts, idx, points[0], points[1], points[2]);
}

/// @brief 円プリミティブ（半径0.5の扇形分割による平面円）
void GenerateCircle(std::vector<Vertex> &verts, std::vector<uint32_t> &idx, char orientation) {
    const PlaneBasis basis = GetPlaneBasis(orientation);
    constexpr int segments = 32;
    const float radius = 0.5f;
    const float pi = GetPI<float>();

    const uint32_t centerIndex = static_cast<uint32_t>(verts.size());
    verts.push_back(MakeVertex(Vector3::Zero(), basis.normal, 0.5f, 0.5f));

    const uint32_t firstRim = centerIndex + 1;
    for (int i = 0; i < segments; ++i) {
        const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const Vector3 p = basis.uAxis * (radius * std::cos(theta)) + basis.vAxis * (radius * std::sin(theta));
        verts.push_back(MakeVertex(p, basis.normal, 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)));
    }
    for (int i = 0; i < segments; ++i) {
        const uint32_t a = firstRim + static_cast<uint32_t>(i);
        const uint32_t b = firstRim + static_cast<uint32_t>((i + 1) % segments);
        // a→b はCCW(外側から見て)の順で並ぶため、そのままの順で積む
        idx.push_back(centerIndex); idx.push_back(a); idx.push_back(b);
    }
}

/// @brief 立方体プリミティブ（1辺1）
void GenerateBox(std::vector<Vertex> &verts, std::vector<uint32_t> &idx) {
    const float h = 0.5f;
    const Vector3 c0(-h, -h, -h);
    const Vector3 c1(h, -h, -h);
    const Vector3 c2(h, h, -h);
    const Vector3 c3(-h, h, -h);
    const Vector3 c4(-h, -h, h);
    const Vector3 c5(h, -h, h);
    const Vector3 c6(h, h, h);
    const Vector3 c7(-h, h, h);

    AddQuad(verts, idx, c4, c5, c6, c7); // +Z
    AddQuad(verts, idx, c1, c0, c3, c2); // -Z
    AddQuad(verts, idx, c1, c2, c6, c5); // +X
    AddQuad(verts, idx, c0, c4, c7, c3); // -X
    AddQuad(verts, idx, c3, c7, c6, c2); // +Y
    AddQuad(verts, idx, c0, c1, c5, c4); // -Y
}

/// @brief 正四面体プリミティブ
void GenerateTetrahedron(std::vector<Vertex> &verts, std::vector<uint32_t> &idx) {
    const float h = 0.5f;
    const Vector3 v0(h, h, h);
    const Vector3 v1(h, -h, -h);
    const Vector3 v2(-h, h, -h);
    const Vector3 v3(-h, -h, h);

    AddTriangle(verts, idx, v0, v1, v2);
    AddTriangle(verts, idx, v0, v2, v3);
    AddTriangle(verts, idx, v0, v3, v1);
    AddTriangle(verts, idx, v1, v3, v2);
}

/// @brief 円柱プリミティブ（半径0.5・高さ1・Y軸沿い）。closedCapsがtrueの場合は上下に蓋を付ける
void GenerateCylinder(std::vector<Vertex> &verts, std::vector<uint32_t> &idx, bool closedCaps) {
    constexpr int segments = 32;
    const float radius = 0.5f;
    const float halfHeight = 0.5f;
    const float pi = GetPI<float>();

    // 側面（スムーズシェーディング）
    for (int i = 0; i < segments; ++i) {
        const float theta0 = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const float theta1 = 2.0f * pi * static_cast<float>(i + 1) / static_cast<float>(segments);
        const Vector3 n0(std::cos(theta0), 0.0f, std::sin(theta0));
        const Vector3 n1(std::cos(theta1), 0.0f, std::sin(theta1));
        const Vector3 b0(radius * n0.x, -halfHeight, radius * n0.z);
        const Vector3 t0(radius * n0.x, halfHeight, radius * n0.z);
        const Vector3 b1(radius * n1.x, -halfHeight, radius * n1.z);
        const Vector3 t1(radius * n1.x, halfHeight, radius * n1.z);
        const float u0 = static_cast<float>(i) / static_cast<float>(segments);
        const float u1 = static_cast<float>(i + 1) / static_cast<float>(segments);

        const uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back(MakeVertex(b0, n0, u0, 1.0f));
        verts.push_back(MakeVertex(t0, n0, u0, 0.0f));
        verts.push_back(MakeVertex(t1, n1, u1, 0.0f));
        verts.push_back(MakeVertex(b1, n1, u1, 1.0f));
        // b0,t0,t1,b1 はCCW(外側から見て)の順で並ぶため、そのままの順で積む
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
    }

    if (!closedCaps) return;

    // 上蓋・下蓋
    const Vector3 topCenter(0.0f, halfHeight, 0.0f);
    const Vector3 bottomCenter(0.0f, -halfHeight, 0.0f);
    const Vector3 topNormal(0.0f, 1.0f, 0.0f);
    const Vector3 bottomNormal(0.0f, -1.0f, 0.0f);
    // 法線+Yを満たす基底 (uAxis.Cross(vAxis) == normal)
    const Vector3 topU(0.0f, 0.0f, 1.0f);
    const Vector3 topV(1.0f, 0.0f, 0.0f);
    // 法線-Yを満たす基底
    const Vector3 bottomU(1.0f, 0.0f, 0.0f);
    const Vector3 bottomV(0.0f, 0.0f, 1.0f);

    for (int cap = 0; cap < 2; ++cap) {
        const Vector3 &center = cap == 0 ? topCenter : bottomCenter;
        const Vector3 &normal = cap == 0 ? topNormal : bottomNormal;
        const Vector3 &uAxis = cap == 0 ? topU : bottomU;
        const Vector3 &vAxis = cap == 0 ? topV : bottomV;

        const uint32_t centerIndex = static_cast<uint32_t>(verts.size());
        verts.push_back(MakeVertex(center, normal, 0.5f, 0.5f));
        const uint32_t firstRim = centerIndex + 1;
        for (int i = 0; i < segments; ++i) {
            const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
            const Vector3 p = center + uAxis * (radius * std::cos(theta)) + vAxis * (radius * std::sin(theta));
            verts.push_back(MakeVertex(p, normal, 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)));
        }
        for (int i = 0; i < segments; ++i) {
            const uint32_t a = firstRim + static_cast<uint32_t>(i);
            const uint32_t b = firstRim + static_cast<uint32_t>((i + 1) % segments);
            idx.push_back(centerIndex); idx.push_back(a); idx.push_back(b);
        }
    }
}

/// @brief UVSphereプリミティブ（半径0.5）
void GenerateUVSphere(std::vector<Vertex> &verts, std::vector<uint32_t> &idx) {
    constexpr int stacks = 16;
    constexpr int slices = 32;
    const float radius = 0.5f;
    const float pi = GetPI<float>();

    const auto position = [&](int i, int j) {
        const float phi = pi * static_cast<float>(j) / static_cast<float>(stacks);
        const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(slices);
        const Vector3 normal(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
        return normal;
    };

    for (int j = 0; j < stacks; ++j) {
        for (int i = 0; i < slices; ++i) {
            const Vector3 n00 = position(i, j);
            const Vector3 n10 = position(i + 1, j);
            const Vector3 n11 = position(i + 1, j + 1);
            const Vector3 n01 = position(i, j + 1);

            const float u0 = static_cast<float>(i) / static_cast<float>(slices);
            const float u1 = static_cast<float>(i + 1) / static_cast<float>(slices);
            const float v0 = static_cast<float>(j) / static_cast<float>(stacks);
            const float v1 = static_cast<float>(j + 1) / static_cast<float>(stacks);

            const uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back(MakeVertex(n00 * radius, n00, u0, v0));
            verts.push_back(MakeVertex(n10 * radius, n10, u1, v0));
            verts.push_back(MakeVertex(n11 * radius, n11, u1, v1));
            verts.push_back(MakeVertex(n01 * radius, n01, u0, v1));
            // (i,j),(i+1,j),(i+1,j+1),(i,j+1) はCCW(外側から見て)の順で並ぶため、そのままの順で積む
            idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
            idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
        }
    }
}

/// @brief ICOSphere（正二十面体分割）用の中間データ
struct RawMesh {
    std::vector<Vector3> positions;
    std::vector<uint32_t> indices;
};

RawMesh GenerateIcosahedron() {
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    RawMesh mesh;
    mesh.positions = {
        Vector3(-1.0f, t, 0.0f), Vector3(1.0f, t, 0.0f), Vector3(-1.0f, -t, 0.0f), Vector3(1.0f, -t, 0.0f),
        Vector3(0.0f, -1.0f, t), Vector3(0.0f, 1.0f, t), Vector3(0.0f, -1.0f, -t), Vector3(0.0f, 1.0f, -t),
        Vector3(t, 0.0f, -1.0f), Vector3(t, 0.0f, 1.0f), Vector3(-t, 0.0f, -1.0f), Vector3(-t, 0.0f, 1.0f),
    };
    for (auto &v : mesh.positions) v = v.Normalize();
    mesh.indices = {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
        1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
        3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
        4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
    };
    return mesh;
}

/// @brief 各辺の中点を球面上に正規化しながら4分割する（面の頂点順・巻き方向は維持される）
RawMesh SubdivideIcosphere(const RawMesh &src) {
    RawMesh dst;
    dst.positions = src.positions;

    std::unordered_map<uint64_t, uint32_t> midpointCache;
    const auto getMidpoint = [&](uint32_t a, uint32_t b) {
        const uint32_t lo = a < b ? a : b;
        const uint32_t hi = a < b ? b : a;
        const uint64_t key = (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
        auto it = midpointCache.find(key);
        if (it != midpointCache.end()) return it->second;

        const Vector3 mid = ((dst.positions[a] + dst.positions[b]) * 0.5f).Normalize();
        const uint32_t newIndex = static_cast<uint32_t>(dst.positions.size());
        dst.positions.push_back(mid);
        midpointCache.emplace(key, newIndex);
        return newIndex;
    };

    for (size_t i = 0; i + 2 < src.indices.size(); i += 3) {
        const uint32_t a = src.indices[i];
        const uint32_t b = src.indices[i + 1];
        const uint32_t c = src.indices[i + 2];
        const uint32_t ab = getMidpoint(a, b);
        const uint32_t bc = getMidpoint(b, c);
        const uint32_t ca = getMidpoint(c, a);
        const uint32_t newIndices[] = { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca };
        dst.indices.insert(dst.indices.end(), std::begin(newIndices), std::end(newIndices));
    }
    return dst;
}

/// @brief ICOSphereプリミティブ（半径0.5、2回分割によるスムーズシェーディング）
void GenerateICOSphere(std::vector<Vertex> &verts, std::vector<uint32_t> &idx) {
    constexpr int subdivisions = 2;
    const float radius = 0.5f;
    const float pi = GetPI<float>();

    RawMesh mesh = GenerateIcosahedron();
    for (int i = 0; i < subdivisions; ++i) {
        mesh = SubdivideIcosphere(mesh);
    }

    verts.reserve(mesh.positions.size());
    for (const auto &normal : mesh.positions) {
        const float u = std::atan2(normal.z, normal.x) / (2.0f * pi) + 0.5f;
        const float v = std::acos(std::clamp(normal.y, -1.0f, 1.0f)) / pi;
        verts.push_back(MakeVertex(normal * radius, normal, u, v));
    }

    // GenerateIcosahedronの面はCCW(外側から見て)の順で並んでおり、このエンジンではその順のまま使用する
    idx = std::move(mesh.indices);
}

void RegisterMesh(const std::string &name, std::vector<Vertex> vertices, std::vector<uint32_t> indices) {
    ModelManager::RegisterProceduralMesh("PrimitiveMesh-" + name, std::move(vertices), std::move(indices));
}

} // namespace

void PrimitiveMeshGenerator::RegisterBuiltinPrimitiveMeshes() {
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateUVSphere(verts, idx);
        RegisterMesh("UVSphere", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateICOSphere(verts, idx);
        RegisterMesh("ICOSphere", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GeneratePlane(verts, idx, 'Z');
        RegisterMesh("Plane-XZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GeneratePlane(verts, idx, 'Y');
        RegisterMesh("Plane-XY", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GeneratePlane(verts, idx, 'X');
        RegisterMesh("Plane-YZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateBox(verts, idx);
        RegisterMesh("Box", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateTriangle(verts, idx, 'Z');
        RegisterMesh("Triangle-XZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateTriangle(verts, idx, 'Y');
        RegisterMesh("Triangle-XY", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateTriangle(verts, idx, 'X');
        RegisterMesh("Triangle-YZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateCircle(verts, idx, 'Z');
        RegisterMesh("Circle-XZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateCircle(verts, idx, 'Y');
        RegisterMesh("Circle-XY", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateCircle(verts, idx, 'X');
        RegisterMesh("Circle-YZ", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateTetrahedron(verts, idx);
        RegisterMesh("Tetrahedron", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateCylinder(verts, idx, true);
        RegisterMesh("CylinderClosed", std::move(verts), std::move(idx));
    }
    {
        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        GenerateCylinder(verts, idx, false);
        RegisterMesh("CylinderOpened", std::move(verts), std::move(idx));
    }
}

} // namespace KashipanEngine
