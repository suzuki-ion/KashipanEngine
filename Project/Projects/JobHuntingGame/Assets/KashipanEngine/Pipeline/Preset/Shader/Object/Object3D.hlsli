#include "../Common/Camera3D.hlsli"

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 worldPosition : WORLDPOSITION;
	uint instanceId : INSTANCEID;
};

struct DirectionalLight {
	uint enabled;
	float4 color;
	float3 direction;
	float intensity;
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
};

struct PointLight {
	uint enabled;
	float4 color;
	float3 position;
	float radius;
	float intensity;
	float decay;
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
};

struct SpotLight {
	uint enabled;
	float4 color;
	float3 position;
	float distance;
	float3 direction;
	float innerAngle;
	float outerAngle;
	float intensity;
	float decay;
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
};

// 面光源（Rect/Sphere/Disc/Tube）。真のLTC等ではなく、形状上の代表点（representative point）法で
// 拡散・鏡面を近似する（AreaLight.hlsli参照）。減衰の考え方はPoint/Spotと同じ式を流用する

struct SphereLight {
	uint enabled;
	float4 color;
	float3 position;
	float radius;        // 減衰範囲（Pointのradiusと同じ意味）
	float sourceRadius;  // 球の物理的な半径
	float intensity;
	float decay;
	int shadowMapIndex;
};

struct DiscLight {
	uint enabled;
	float4 color;
	float3 position;
	float distance;      // 減衰範囲（Spotのdistanceと同じ意味）
	float3 direction;    // 発光面の法線（片面発光）
	float sourceRadius;  // ディスクの物理的な半径
	float intensity;
	float decay;
	int shadowMapIndex;
};

struct RectLight {
	uint enabled;
	float4 color;
	float3 position;
	float distance;      // 減衰範囲（Spotのdistanceと同じ意味）
	float3 direction;    // 発光面の法線（片面発光）
	float3 right;        // 幅方向の軸（正規化済み）
	float width;
	float3 up;           // 高さ方向の軸（正規化済み）
	float height;
	float intensity;
	float decay;
	int shadowMapIndex;
};

struct TubeLight {
	uint enabled;
	float4 color;
	float3 p0;            // チューブの端点0
	float radius;         // 減衰範囲（Pointのradiusと同じ意味）
	float3 p1;            // チューブの端点1
	float sourceRadius;   // チューブ（円柱）の物理的な半径
	float intensity;
	float decay;
	int shadowMapIndex;
};

// 全方位（体積）発光のボックスライト。ボックス表面上の最近接点を代表点として、Tubeと同じ扱いで
// 拡散・鏡面・減衰を求める（AreaLight.hlsli の BoxClosestPoint 参照）。影もPoint/Sphere/Tubeと
// 同じキューブ6面（中心からの近似）で扱う
struct BoxLight {
	uint enabled;
	float4 color;
	float3 position;      // ボックス中心
	float radius;         // ボックス表面からさらに届く減衰距離（Pointのradiusと同じ意味）
	float3 right;         // ローカルX軸（正規化済み）
	float halfWidth;
	float3 up;            // ローカルY軸（正規化済み）
	float halfHeight;
	float3 forward;       // ローカルZ軸（正規化済み）
	float halfDepth;
	float intensity;
	float decay;
	int shadowMapIndex;
};
