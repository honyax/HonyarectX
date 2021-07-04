// 頂点シェーダ→ピクセルシェーダへのやり取りに使用する構造体
struct BasicType {
	float4 svpos:SV_POSITION;	// システム用頂点座標
	float4 normal:NORMAL;		// 法線ベクトル
	float2 uv:TEXCOORD;			// UV座標
};

Texture2D<float4> tex : register(t0);	// 0番スロットに設定されたテクスチャ
SamplerState smp : register(s0);		// 0番スロットに設定されたサンプラー

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
	matrix world;				// ワールド変換強烈
	matrix viewproj;			// ビュープロジェクション行列
}
