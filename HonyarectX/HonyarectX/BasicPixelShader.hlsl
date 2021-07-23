#include "BasicShaderHeader.hlsli"

float4 BasicPS(BasicType input) : SV_TARGET
{
	float3 light = normalize(float3(1, -1, 1));
	float brightness = dot(-light, input.normal.xyz);
	float2 sphereMapUV = input.vnormal.xy;
	return float4(brightness, brightness, brightness, 1)	// 輝度
		* diffuse						// ディフューズ色
		* tex.Sample(smp, input.uv)		// テクスチャカラー
		* sph.Sample(smp, sphereMapUV)		// スフィアマップ（乗算）
		+ spa.Sample(smp, sphereMapUV)		// スフィアマップ（加算）
		;
}
