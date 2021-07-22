// 頂点シェーダ→ピクセルシェーダへのやり取りに使用する構造体
struct BasicType {
	float4 svpos:SV_POSITION;	// システム用頂点座標
	float4 normal:NORMAL;		// 法線ベクトル
	float2 uv:TEXCOORD;			// UV座標
};

Texture2D<float4> tex : register(t0);	// 0番スロットに設定されたテクスチャ
SamplerState smp : register(s0);		// 0番スロットに設定されたサンプラー

// 定数バッファー0
cbuffer cbuff0 : register(b0)
{
	matrix world;				// ワールド変換強烈
	matrix viewproj;			// ビュープロジェクション行列
};

// 定数バッファー1
// マテリアル用
cbuffer Material : register(b1)
{
	float4 diffuse;				// ディフューズ色
	float4 specular;			// スペキュラ
	float3 ambient;				// アンビエント
};
