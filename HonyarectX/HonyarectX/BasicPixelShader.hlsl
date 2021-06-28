#include "BasicShaderHeader.hlsli"

float4 BasicPS(BasicType input) : SV_TARGET
{
	return float4(input.uv, 1.0f, 1.0f);
}
