// 頂点シェーダ→ピクセルシェーダへのやり取りに使用する構造体
struct BasicType {
	float4 svpos : SV_POSITION;	// システム用頂点座標
	float4 pos : POSITION;		// 頂点座標
	float4 normal : NORMAL0;	// 法線ベクトル
	float4 vnormal : NORMAL1;	// ビュー変換後の法線ベクトル
	float2 uv : TEXCOORD;		// UV座標
	float3 ray : VECTOR;		// 視点
};

Texture2D<float4> tex : register(t0);	// 0番スロットに設定されたテクスチャ（ベース）
Texture2D<float4> sph : register(t1);	// 1番スロットに設定されたテクスチャ（乗算）
Texture2D<float4> spa : register(t2);	// 2番スロットに設定されたテクスチャ（加算）
Texture2D<float4> toon : register(t3);	// 3番スロットに設定されたテクスチャ（トゥーン）
SamplerState smp : register(s0);		// 0番スロットに設定されたサンプラー
SamplerState smpToon : register(s1);	// 1番スロットに設定されたサンプラー（トゥーン用）

// 定数バッファー0
cbuffer cbuff0 : register(b0)
{
	matrix view;				// ビュー行列
	matrix proj;				// プロジェクション行列
	float3 eye;					// 視点
};
cbuffer Transform : register(b1)
{
	matrix world;				// ワールド変換強烈
	matrix bones[256];			// ボーン行列
}

// 定数バッファー1
// マテリアル用
cbuffer Material : register(b2)
{
	float4 diffuse;				// ディフューズ色
	float4 specular;			// スペキュラ
	float3 ambient;				// アンビエント
};
