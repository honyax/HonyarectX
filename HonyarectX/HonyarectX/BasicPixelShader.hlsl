#include "BasicShaderHeader.hlsli"

float4 BasicPS(BasicType input) : SV_TARGET
{
	//return float4(tex.Sample(smp, input.uv));
	float3 light = normalize(float3(1, -1, 1));
	float brightness = dot(-light, input.normal.xyz);
	return float4(brightness, brightness, brightness, 1) * diffuse;
}
