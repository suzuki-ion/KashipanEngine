#pragma once

// 面光源（Rect/Sphere/Disc/Tube）の拡散・鏡面近似ユーティリティ。
//
// 真のLTC（Linearly Transformed Cosines）は、GGX等のBRDFに対して外部で数値フィッティングされた
// 専用LUTテクスチャを前提とするため、ここでは採用していない。代わりに、Karis "Real Shading in
// Unreal Engine 4"(2013) のsphere/tubeライト近似（形状上の最近接点＝代表点をBlinn-Phongの
// 光源方向として扱い、光源サイズに応じてハイライトを広げる）を disc/rect にも拡張した
// 「代表点（representative point）法」で統一的に扱う。
//
// 拡散照明はPoint/Spotと同じ減衰式（pow(saturate(1 - dist/range), decay)）を、
// 光源中心（Rect/Discは平面上の中心点）への方向で評価する簡易近似。
// 面としての形状は主に鏡面ハイライトの広がりと、PCSS半影のソフトさ（光源サイズ）に反映される。

/// @brief 光源サイズに応じてBlinn-Phongのshininessを弱める（大きい/近い光源ほどハイライトが広がる）
/// @param sourceRadius 光源の物理的な半径相当（Rectは幅高さの半分の大きい方）
/// @param distanceToLight 表面から光源（中心）までの距離
inline float AreaLightAdjustedShininess(float shininess, float sourceRadius, float distanceToLight) {
	float sinAlpha = saturate(sourceRadius / max(distanceToLight, 1e-3f));
	// sinAlphaが1に近い（光源が大きい/近い）ほどハイライトを弱めて広げる
	return lerp(shininess, 1.0f, sinAlpha);
}

/// @brief 反射ベクトルと球面の最近接点を代表点として返す（Karis 2013 のsphereライト近似）
/// @param reflectDir 表面からの反射ベクトル（正規化済み）
inline float3 SphereRepresentativePoint(float3 worldPos, float3 reflectDir, float3 sphereCenter, float sphereRadius) {
	float3 toCenter = sphereCenter - worldPos;
	float t = max(dot(toCenter, reflectDir), 0.0f);
	float3 closestOnRay = worldPos + reflectDir * t;
	float3 centerToClosest = closestOnRay - sphereCenter;
	float centerToClosestDist = length(centerToClosest);
	if (centerToClosestDist > 1e-4f) {
		return sphereCenter + centerToClosest * (sphereRadius / centerToClosestDist);
	}
	return sphereCenter + float3(0.0f, sphereRadius, 0.0f);
}

/// @brief レイと平面の交点を求める（交差しない場合はfalseを返し、hitPointはplanePointのまま）
inline bool RayPlaneIntersect(float3 rayOrigin, float3 rayDir, float3 planePoint, float3 planeNormal, out float3 hitPoint) {
	hitPoint = planePoint;
	float denom = dot(rayDir, planeNormal);
	if (abs(denom) < 1e-5f) {
		return false;
	}
	float t = dot(planePoint - rayOrigin, planeNormal) / denom;
	if (t < 0.0f) {
		return false;
	}
	hitPoint = rayOrigin + rayDir * t;
	return true;
}

/// @brief 反射ベクトルとディスク平面の交点をディスク範囲内へクランプした代表点を返す
inline float3 DiscRepresentativePoint(float3 worldPos, float3 reflectDir, float3 center, float3 normal, float discRadius) {
	float3 hit;
	if (RayPlaneIntersect(worldPos, reflectDir, center, normal, hit)) {
		float3 offset = hit - center;
		float d = length(offset);
		if (d > discRadius && d > 1e-5f) {
			offset *= (discRadius / d);
		}
		return center + offset;
	}
	return center;
}

/// @brief 反射ベクトルと矩形平面の交点を矩形範囲内へクランプした代表点を返す
inline float3 RectRepresentativePoint(float3 worldPos, float3 reflectDir, float3 center, float3 normal,
	float3 right, float3 up, float halfWidth, float halfHeight) {
	float3 hit;
	if (RayPlaneIntersect(worldPos, reflectDir, center, normal, hit)) {
		float3 offset = hit - center;
		float x = clamp(dot(offset, right), -halfWidth, halfWidth);
		float y = clamp(dot(offset, up), -halfHeight, halfHeight);
		return center + right * x + up * y;
	}
	return center;
}

/// @brief 表面から見てチューブ（線分）上で最も近い点を返す（拡散・鏡面双方の基準点として使う）
inline float3 TubeClosestPoint(float3 worldPos, float3 p0, float3 p1) {
	float3 ab = p1 - p0;
	float abLenSq = max(dot(ab, ab), 1e-6f);
	float t = saturate(dot(worldPos - p0, ab) / abLenSq);
	return p0 + ab * t;
}

/// @brief 表面から見てボックス（直方体）上で最も近い点を返す（拡散・鏡面双方の基準点として使う）
/// @details ワールド座標をボックスのローカル軸へ射影し、半径（半幅・半高・半奥行き）でクランプする。
///          点がボックス内部にある場合は各軸ともクランプが効かないため、そのまま内部の点が返る
///          （TubeClosestPointと同じ「最近接点法」をボックスへ拡張したもの）
inline float3 BoxClosestPoint(float3 worldPos, float3 center, float3 right, float3 up, float3 forward, float3 halfExtents) {
	float3 d = worldPos - center;
	float x = clamp(dot(d, right), -halfExtents.x, halfExtents.x);
	float y = clamp(dot(d, up), -halfExtents.y, halfExtents.y);
	float z = clamp(dot(d, forward), -halfExtents.z, halfExtents.z);
	return center + right * x + up * y + forward * z;
}
