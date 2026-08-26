// Object2DWorldが定義されている場合、エディターのシーンビューが2Dオブジェクトを3D空間内に
// 配置・選択・ギズモ編集できるようにするための特殊バリアント（PipelineVariantResolverの
// Worldトークン、SceneRenderer.cppのResolveEditorWorldPipelineName参照）。
// gCamera2D/gCamera3Dはどちらもregister(b0)だが排他利用のため競合しない
#ifdef Object2DWorld
#include "../Common/Camera3D.hlsli"
#else
#include "../Common/Camera2D.hlsli"
#endif
#include "../Common/Time.hlsli"

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	uint instanceId : INSTANCEID;
};
