// カスタムシェーダー向けの追加パラメータは、この構造体の末尾に自由に追加してよい（Material3D.hlsli参照）。
struct Material {
	float4 ghostColor; // @Color
};

struct GhostVSOutput {
	float4 position : SV_POSITION;
	uint instanceId : INSTANCEID;
};
