#include "BasicShaderHeader.hlsli"

float4 BasicPS(BasicType input) : SV_TARGET
{
	// 光の向かうベクトル（平行光線）
	float3 light = normalize(float3(1, -1, 1));

	// ライトのカラー（1, 1, 1で真っ白）
	float3 lightColor = float3(1, 1, 1);

	// ディフューズ計算
	float diffuseB = dot(-light, input.normal.xyz);

	// 光の反射ベクトル
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// スフィアマップ用uv
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	// テクスチャカラー
	float4 texColor = tex.Sample(smp, input.uv);

	return max(diffuseB								// 輝度
		* diffuse									// ディフューズ色
		* texColor									// テクスチャカラー
		* sph.Sample(smp, sphereMapUV)				// スフィアマップ（乗算）
		+ spa.Sample(smp, sphereMapUV) * texColor	// スフィアマップ（加算）
		+ float4(specularB * specular.rgb, 1)		// スペキュラ
		, float4(texColor.xyz * ambient, 1)				// アンビエント
	);
}
