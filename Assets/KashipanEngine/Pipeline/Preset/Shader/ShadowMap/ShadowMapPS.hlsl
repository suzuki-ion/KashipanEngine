#include "../Object/Object3D.hlsli"

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput o;
	o.color = float4(1, 1, 1, 1);
    return o;
}
