#include "../Object/Object3D.hlsli"

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput o;
	o.color = 0.0f;
    return o;
}
